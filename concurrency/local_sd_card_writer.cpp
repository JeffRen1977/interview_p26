// Problem: Local SD Card Writer + fan-out camera pipeline (I/O bound)
//
 // Scenario:
 //   One ISP producer captures an 8MB frame once. Fan-out the *same*
 //   shared_ptr to parallel consumers:
 //     - H.264 encoder (must not drop; may block)
 //     - AI detector (latency-sensitive; drop oldest if busy)
 //     - SD card writer (reliable local I/O; may block)
 //
 // What interviewers look for:
 //   - No pixel memcpy / no duplicate 8MB buffers → shared_ptr / refcount
 //   - Per-consumer queues with different backpressure policies
 //   - Correct CV wakeups for bounded queues (notify after pop AND push)
 //
 // Filename note: was loca_sd_card_writer.cpp (typo).

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

struct Frame {
    std::uint64_t frame_id{0};
    std::vector<std::uint8_t> pixel_data;

    explicit Frame(std::uint64_t id, std::size_t bytes = 64)
        : frame_id(id), pixel_data(bytes) {}
};

template <typename T>
class BoundedQueue {
 public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        assert(capacity_ > 0);
    }

    // Blocks while full. Used for reliable sinks (encoder / SD).
    void push_blocking(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || shutdown_;
        });
        if (shutdown_) {
            return;
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();
    }

    // Never blocks: if full, drop oldest then push. Used for AI.
    void push_drop_oldest(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
            return;
        }
        if (queue_.size() >= capacity_) {
            queue_.pop();
            ++dropped_;
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();
    }

    // Returns false on shutdown + empty.
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();  // critical: wake push_blocking waiters
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t dropped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

 private:
    const std::size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    bool shutdown_{false};
    std::size_t dropped_{0};
};

class CameraPipeline {
 public:
    CameraPipeline() {
        h264_ = std::thread([this] { drain(h264_q_, "H264"); });
        ai_ = std::thread([this] { drain(ai_q_, "AI"); });
        sd_ = std::thread([this] { drain(sd_q_, "SD"); });
    }

    ~CameraPipeline() { stop(); }

    // Fan-out: same shared_ptr, three queues → refcount, no 8MB copies.
    void dispatch(std::shared_ptr<Frame> frame) {
        h264_q_.push_blocking(frame);
        ai_q_.push_drop_oldest(frame);
        sd_q_.push_blocking(frame);
    }

    void stop() {
        h264_q_.shutdown();
        ai_q_.shutdown();
        sd_q_.shutdown();
        if (h264_.joinable()) {
            h264_.join();
        }
        if (ai_.joinable()) {
            ai_.join();
        }
        if (sd_.joinable()) {
            sd_.join();
        }
    }

    std::size_t ai_dropped() const { return ai_q_.dropped(); }

 private:
    void drain(BoundedQueue<std::shared_ptr<Frame>>& q, const char* name) {
        std::shared_ptr<Frame> frame;
        while (q.pop(frame)) {
            (void)name;
            // Simulate work; frame dies when last consumer releases shared_ptr.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++processed_;
        }
    }

    BoundedQueue<std::shared_ptr<Frame>> h264_q_{4};
    BoundedQueue<std::shared_ptr<Frame>> ai_q_{1};  // tiny → forces drops
    BoundedQueue<std::shared_ptr<Frame>> sd_q_{4};

    std::thread h264_;
    std::thread ai_;
    std::thread sd_;
    std::atomic<int> processed_{0};
};

bool test_queue_cv_wakeup() {
    BoundedQueue<int> q(1);
    std::thread producer([&] {
        q.push_blocking(1);
        q.push_blocking(2);  // blocks until consumer pops
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int v = 0;
    assert(q.pop(v) && v == 1);
    assert(q.pop(v) && v == 2);
    producer.join();
    q.shutdown();
    return true;
}

int main() {
    assert(test_queue_cv_wakeup());

    CameraPipeline pipeline;
    for (std::uint64_t i = 1; i <= 8; ++i) {
        auto frame = std::make_shared<Frame>(i);
        // use_count == 1 before fan-out; after three pushes (copies) >= 1.
        pipeline.dispatch(std::move(frame));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    pipeline.stop();

    std::cout << "local_sd_card_writer: ok (ai_dropped=" << pipeline.ai_dropped()
              << ")\n";
    return 0;
}
