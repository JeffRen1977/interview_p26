// Problem: Producer-Consumer with Frame Dropping (real-time video)
//
// Scenario:
//   ISP produces frames at 30 FPS; AI inference only runs ~10 FPS.
//   Consumer must always process the *latest* frame; older pending frames
//   are dropped. Producer must NEVER block on a slow consumer.
//
// Design:
//   Single-slot "latest frame" buffer. publish() overwrites the slot;
//   get_latest() waits until a new frame arrives, then takes ownership.
//
// Correctness rules:
//   - Transfer exclusive ownership (unique_ptr). Do NOT share one Frame
 //     object between producer write and consumer read (use-after-reuse).
 //   - Dropping = overwriting the slot; discarded unique_ptr frees the frame.
 //
 // Follow-ups: triple-buffering; dma-buf FD instead of pixels; drop-rate metrics.

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

struct Frame {
    std::uint64_t frame_id{0};
    std::uint64_t timestamp_ms{0};
    std::vector<std::uint8_t> payload;

    explicit Frame(std::size_t bytes = 64) : payload(bytes) {}
};

class LatestFrameBuffer {
 public:
    // Non-blocking for the producer: overwrite any unread frame.
    void publish(std::unique_ptr<Frame> frame) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // Previous unread frame is dropped here (unique_ptr destroyed).
            slot_ = std::move(frame);
            has_new_ = true;
            ++published_;
        }
        cv_.notify_one();
    }

    // Blocks until a new frame exists (or shutdown). Takes ownership.
    std::unique_ptr<Frame> get_latest() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return has_new_ || shutdown_; });
        if (!has_new_) {
            return nullptr;  // shutdown with nothing left
        }
        has_new_ = false;
        ++consumed_;
        return std::move(slot_);
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    std::uint64_t published() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return published_;
    }

    std::uint64_t consumed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return consumed_;
    }

 private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::unique_ptr<Frame> slot_;
    bool has_new_{false};
    bool shutdown_{false};
    std::uint64_t published_{0};
    std::uint64_t consumed_{0};
};

bool test_overwrite_semantics() {
    LatestFrameBuffer buf;

    auto make = [](std::uint64_t id) {
        auto f = std::make_unique<Frame>();
        f->frame_id = id;
        return f;
    };

    buf.publish(make(1));
    buf.publish(make(2));
    buf.publish(make(3));

    auto got = buf.get_latest();
    assert(got != nullptr);
    assert(got->frame_id == 3);  // 1 and 2 were dropped
    assert(buf.published() == 3);
    assert(buf.consumed() == 1);

    buf.publish(make(4));
    got = buf.get_latest();
    assert(got && got->frame_id == 4);

    buf.shutdown();
    assert(buf.get_latest() == nullptr);
    return true;
}

int main() {
    assert(test_overwrite_semantics());

    LatestFrameBuffer channel;
    constexpr int kProduce = 30;

    std::thread producer([&] {
        for (int i = 1; i <= kProduce; ++i) {
            auto frame = std::make_unique<Frame>();
            frame->frame_id = static_cast<std::uint64_t>(i);
            frame->timestamp_ms = static_cast<std::uint64_t>(i * 33);
            channel.publish(std::move(frame));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        channel.shutdown();
    });

    std::thread consumer([&] {
        std::uint64_t last = 0;
        while (true) {
            auto frame = channel.get_latest();
            if (!frame) {
                break;
            }
            assert(frame->frame_id > last);  // monotonically newer
            last = frame->frame_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        assert(last >= 1);
    });

    producer.join();
    consumer.join();

    assert(channel.published() == static_cast<std::uint64_t>(kProduce));
    assert(channel.consumed() <= channel.published());
    assert(channel.consumed() < channel.published());  // some drops expected

    std::cout << "producer_consumer_frame_dropping: ok (published="
              << channel.published() << " consumed=" << channel.consumed()
              << ")\n";
    return 0;
}
