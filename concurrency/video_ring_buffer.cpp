// Problem: In-Memory Video Ring Buffer (rolling pre-event clip)
//
// Scenario (Verkada-style):
//   Camera continuously records into a fixed-size ring so that when motion
 //   fires, you can upload the last N seconds *before* the event without
 //   copying pixel payloads.
 //
 // Stages to explain on the whiteboard:
 //   1) Idle: pre-allocate frame buffers into a FreeList (no runtime new).
 //   2) Produce: Recorder acquires a buffer, fills pixels.
 //   3) Cache: push shared_ptr into the ring (overwrite oldest when full).
 //   4) Upload: snapshot() copies shared_ptrs only (refcount +1), zero memcpy.
 //   5) Recycle: when upload releases and ring overwrote the slot, custom
 //      deleter returns the Frame to the FreeList.
 //
 // Correctness:
 //   - FreeList via shared_ptr + custom deleter (weak_ptr to pool).
 //   - snapshot() returns chronological order (oldest → newest).
 //   - Overwritten frames still alive if upload holds a shared_ptr.

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

struct Frame {
    int id{0};
};

class FreeList : public std::enable_shared_from_this<FreeList> {
 public:
    explicit FreeList(int capacity) {
        for (int i = 0; i < capacity; ++i) {
            pool_.push(std::make_unique<Frame>());
        }
    }

    std::shared_ptr<Frame> acquire() {
        std::unique_ptr<Frame> raw;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pool_.empty()) {
                return nullptr;
            }
            raw = std::move(pool_.front());
            pool_.pop();
        }

        std::weak_ptr<FreeList> weak_self = shared_from_this();
        return std::shared_ptr<Frame>(
            raw.release(),
            [weak_self](Frame* ptr) {
                const int id = ptr->id;
                if (auto self = weak_self.lock()) {
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    self->pool_.push(std::unique_ptr<Frame>(ptr));
                    std::cout << "  [FreeList] recycled frame #" << id << "\n";
                } else {
                    delete ptr;
                }
            });
    }

    std::size_t free_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

 private:
    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<Frame>> pool_;
};

class VideoRingBuffer {
 public:
    explicit VideoRingBuffer(std::size_t capacity)
        : capacity_(capacity), slots_(capacity), count_(0), head_(0) {}

    // Recorder thread: store frame; when full, overwrite oldest.
    void push(std::shared_ptr<Frame> frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        slots_[head_] = std::move(frame);
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) {
            ++count_;
        }
    }

    // Upload thread: oldest → newest shared_ptr copies (no pixel copy).
    std::vector<std::shared_ptr<Frame>> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Frame>> clip;
        clip.reserve(count_);

        // head_ is next write index = oldest slot when the ring is full.
        const std::size_t start =
            (count_ == capacity_) ? head_ : (head_ + capacity_ - count_) % capacity_;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& frame = slots_[(start + i) % capacity_];
            if (frame) {
                clip.push_back(frame);
            }
        }
        return clip;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

 private:
    const std::size_t capacity_;
    std::vector<std::shared_ptr<Frame>> slots_;
    std::size_t count_;
    std::size_t head_;
    mutable std::mutex mutex_;
};

bool test_video_ring_buffer() {
    auto pool = std::make_shared<FreeList>(3);
    VideoRingBuffer ring(2);

    assert(pool->free_count() == 3);

    auto f1 = pool->acquire();
    f1->id = 101;
    ring.push(f1);
    f1.reset();

    auto f2 = pool->acquire();
    f2->id = 102;
    ring.push(f2);
    f2.reset();

    assert(pool->free_count() == 1);
    assert(ring.size() == 2);

    auto clip = ring.snapshot();
    assert(clip.size() == 2);
    assert(clip[0]->id == 101);
    assert(clip[1]->id == 102);

    // Overwrite slot holding 101 while upload still holds it → no recycle yet.
    auto f3 = pool->acquire();
    f3->id = 103;
    ring.push(f3);
    f3.reset();
    assert(pool->free_count() == 0);

    auto clip2 = ring.snapshot();
    assert(clip2.size() == 2);
    assert(clip2[0]->id == 102);
    assert(clip2[1]->id == 103);

    clip.clear();  // releases 101 → recycled
    assert(pool->free_count() == 1);

    clip2.clear();
    // ring still holds 102,103 → free_count stays 1 until overwritten/released
    assert(pool->free_count() == 1);

    return true;
}

int main() {
    assert(test_video_ring_buffer());
    std::cout << "video_ring_buffer: ok\n";
    return 0;
}
