#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// ~8MB payload; passed by shared_ptr so fan-out is a refcount bump, not a copy,
// and a consumer that grabbed the pointer keeps the frame alive even after the
// producer reuses the slot.
struct Frame {
    uint64_t             sequenceId;
    int64_t              captureTimestampNs;
    int                  width;
    int                  height;
    std::vector<uint8_t> pixels;
};
using FramePtr = std::shared_ptr<const Frame>;

enum class ConsumerPolicy {
    Reliable,  // gates the producer (backpressure); never drops — encoder, SD writer
    Lossy      // never gates producer; skips to newest on lag — AI detector
};

struct ConsumerStats {
    uint64_t framesConsumed;
    uint64_t framesDropped;
    uint64_t readCursor;
    uint64_t producerCursor;
};

class FrameRing {
public:
    explicit FrameRing(size_t capacity)
        : capacity_(capacity), slots_(capacity) {}

    class Consumer {
    public:
        // Blocks until a frame is available (or the ring is closed and drained).
        // Reliable: delivers the next frame in strict sequence.
        // Lossy:    delivers the newest frame, dropping everything older.
        bool receive(FramePtr& out) { return ring_->receive(state_, out, /*block=*/true); }
        bool tryReceive(FramePtr& out) { return ring_->receive(state_, out, /*block=*/false); }
        ConsumerStats stats() const { return ring_->statsFor(state_); }

    private:
        friend class FrameRing;
        Consumer(FrameRing* ring, std::shared_ptr<struct ConsumerState> state)
            : ring_(ring), state_(std::move(state)) {}
        FrameRing*                           ring_;
        std::shared_ptr<struct ConsumerState> state_;
    };

    std::shared_ptr<Consumer> subscribe(ConsumerPolicy policy) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto state = std::make_shared<ConsumerState>();
        state->policy = policy;
        state->cursor = writeSeq_;  // only see frames published from now on
        consumers_.push_back(state);
        return std::shared_ptr<Consumer>(new Consumer(this, state));
    }

    // Producer (single writer). Blocks while a reliable consumer hasn't caught up.
    void publish(FramePtr frame) {
        std::unique_lock<std::mutex> lock(mutex_);
        producerCv_.wait(lock, [this] {
            return (writeSeq_ - reliableBarrierLocked()) < capacity_;
        });
        slots_[writeSeq_ % capacity_] = std::move(frame);
        ++writeSeq_;
        dataCv_.notify_all();
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        dataCv_.notify_all();
    }

private:
    struct ConsumerState {
        ConsumerPolicy policy = ConsumerPolicy::Reliable;
        uint64_t       cursor = 0;
        uint64_t       framesConsumed = 0;
        uint64_t       framesDropped = 0;
    };

    // Write barrier = oldest slot still needed by any RELIABLE consumer.
    // With no reliable consumers this returns writeSeq_ → producer never blocks.
    uint64_t reliableBarrierLocked() const {
        uint64_t barrier = writeSeq_;
        for (const auto& consumer : consumers_) {
            if (consumer->policy == ConsumerPolicy::Reliable) {
                barrier = std::min(barrier, consumer->cursor);
            }
        }
        return barrier;
    }

    bool receive(const std::shared_ptr<ConsumerState>& state, FramePtr& out, bool block) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (block) {
            dataCv_.wait(lock, [&] { return state->cursor < writeSeq_ || closed_; });
        }
        if (state->cursor >= writeSeq_) {
            return false;  // nothing available (closed & drained, or non-blocking miss)
        }

        if (state->policy == ConsumerPolicy::Lossy) {
            // Freshness: jump to the newest published frame, counting skipped ones.
            const uint64_t newest = writeSeq_ - 1;
            state->framesDropped += newest - state->cursor;
            state->cursor = newest;
        }

        out = slots_[state->cursor % capacity_];
        ++state->cursor;
        ++state->framesConsumed;

        // Advancing a reliable cursor frees a slot → wake the producer.
        if (state->policy == ConsumerPolicy::Reliable) {
            producerCv_.notify_one();
        }
        return true;
    }

    ConsumerStats statsFor(const std::shared_ptr<ConsumerState>& state) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {state->framesConsumed, state->framesDropped, state->cursor, writeSeq_};
    }

    const size_t                                     capacity_;
    std::vector<FramePtr>                            slots_;
    std::vector<std::shared_ptr<ConsumerState>>      consumers_;
    uint64_t                                         writeSeq_ = 0;
    bool                                             closed_ = false;
    mutable std::mutex                               mutex_;
    std::condition_variable                          producerCv_;  // waits for slot space
    std::condition_variable                          dataCv_;      // waits for new frames
};
