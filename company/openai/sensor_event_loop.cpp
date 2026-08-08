// C++ twin of sensor_event_loop.py — Embedded multi-sensor Event Loop demo.
//
// Design doc: embedded-sensor-event-loop.md
// Python twin: sensor_event_loop.py
//
// Highlights vs Python:
// - Real acquire/release atomics on SPSC head/tail
// - alignas(64) to avoid False Sharing
// - condition_variable models epoll_wait / futex sleep

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

constexpr std::size_t kCacheLine = 64;

template <typename T, std::size_t Capacity>
class LockFreeSpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be power of 2");

 public:
    bool push(const T& item) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        // Free-running counters: full when Cap items are in flight.
        if (tail - head == Capacity) {
            return false;
        }
        buffer_[tail & kMask] = item;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) {
            return std::nullopt;
        }
        T item = buffer_[head & kMask];
        head_.store(head + 1, std::memory_order_release);
        return item;
    }

 private:
    static constexpr std::size_t kMask = Capacity - 1;
    T buffer_[Capacity]{};
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

enum class SensorType { Imu, Camera, Audio };

struct SensorEvent {
    SensorType type = SensorType::Imu;
    double timestamp_ms = 0.0;
    std::int64_t payload_id = 0;  // stand-in for dma-buf / frame id
};

struct FusedFrame {
    double cam_t_ms = 0.0;
    std::int64_t cam_payload = 0;
    int imu_count = 0;
};

class EventLoop {
 public:
    void dispatch(const SensorEvent& event) {
        bool ok = true;
        switch (event.type) {
            case SensorType::Imu:
                ok = imu_q_.push(event);
                if (!ok) {
                    ++drops_imu_;
                }
                break;
            case SensorType::Camera:
                ok = cam_q_.push(event);
                if (!ok) {
                    ++drops_cam_;
                }
                break;
            case SensorType::Audio:
                ok = audio_q_.push(event);
                if (!ok) {
                    ++drops_audio_;
                }
                break;
        }
        wakeup();
    }

    void stop() {
        running_.store(false, std::memory_order_relaxed);
        wakeup();
    }

    void run() {
        int idle_spins = 0;
        while (running_.load(std::memory_order_relaxed)) {
            bool processed = false;

            // Drain IMU first (highest rate / lowest latency path).
            while (auto ev = imu_q_.pop()) {
                imu_history_.push_back(*ev);
                if (imu_history_.size() > 512) {
                    imu_history_.pop_front();
                }
                processed = true;
            }

            while (auto ev = cam_q_.pop()) {
                fuse_camera(*ev);
                processed = true;
            }

            while (audio_q_.pop()) {
                processed = true;
            }

            if (processed) {
                idle_spins = 0;
                continue;
            }

            ++idle_spins;
            if (idle_spins < 3) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            } else {
                // Power-saving sleep — production: epoll_wait / futex.
                std::unique_lock lock(wake_mu_);
                wake_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                    return wake_pending_ ||
                           !running_.load(std::memory_order_relaxed);
                });
                wake_pending_ = false;
                idle_spins = 0;
            }
        }
    }

    const std::vector<FusedFrame>& fused() const { return fused_; }
    int drops_imu() const { return drops_imu_; }
    int drops_cam() const { return drops_cam_; }
    int drops_audio() const { return drops_audio_; }

 private:
    void wakeup() {
        {
            std::lock_guard lock(wake_mu_);
            wake_pending_ = true;
        }
        wake_cv_.notify_one();
    }

    void fuse_camera(const SensorEvent& cam) {
        constexpr double kWindowMs = 2.5;
        int matched = 0;
        for (const auto& imu : imu_history_) {
            if (std::fabs(imu.timestamp_ms - cam.timestamp_ms) <= kWindowMs) {
                ++matched;
            }
        }
        fused_.push_back(
            FusedFrame{cam.timestamp_ms, cam.payload_id, matched});
    }

    std::atomic<bool> running_{true};

    LockFreeSpscQueue<SensorEvent, 256> imu_q_;
    LockFreeSpscQueue<SensorEvent, 32> cam_q_;
    LockFreeSpscQueue<SensorEvent, 64> audio_q_;

    std::mutex wake_mu_;
    std::condition_variable wake_cv_;
    bool wake_pending_ = false;

    std::deque<SensorEvent> imu_history_;
    std::vector<FusedFrame> fused_;
    int drops_imu_ = 0;
    int drops_cam_ = 0;
    int drops_audio_ = 0;
};

static void producer(EventLoop& loop, SensorType type, double hz,
                     double duration_s) {
    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::duration<double>(1.0 / hz);
    const auto t0 = clock::now();
    std::int64_t n = 0;

    while (std::chrono::duration<double>(clock::now() - t0).count() <
           duration_s) {
        const double now_ms =
            std::chrono::duration<double, std::milli>(clock::now() - t0)
                .count();
        loop.dispatch(SensorEvent{type, now_ms, n});
        ++n;
        std::this_thread::sleep_for(period);
    }
}

int main() {
    EventLoop loop;
    std::thread loop_thread([&] { loop.run(); });

    constexpr double kDuration = 0.35;
    std::thread imu([&] { producer(loop, SensorType::Imu, 200.0, kDuration); });
    std::thread cam(
        [&] { producer(loop, SensorType::Camera, 30.0, kDuration); });
    std::thread audio(
        [&] { producer(loop, SensorType::Audio, 50.0, kDuration); });

    imu.join();
    cam.join();
    audio.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.stop();
    loop_thread.join();

    const auto& fused = loop.fused();
    assert(!fused.empty());

    int with_imu = 0;
    for (const auto& f : fused) {
        if (f.imu_count > 0) {
            ++with_imu;
        }
    }
    assert(with_imu >= static_cast<int>(fused.size()) / 2);

    const auto& sample = fused[fused.size() / 2];
    std::cout << "fused_frames=" << fused.size() << " with_imu=" << with_imu
              << "\n";
    std::cout << "drops={imu:" << loop.drops_imu()
              << ", cam:" << loop.drops_cam()
              << ", audio:" << loop.drops_audio() << "}\n";
    std::cout << "sample: cam_t=" << sample.cam_t_ms
              << " payload=" << sample.cam_payload
              << " imu_count=" << sample.imu_count << "\n";
    std::cout << "sensor_event_loop_cpp: ok\n";
    return 0;
}
