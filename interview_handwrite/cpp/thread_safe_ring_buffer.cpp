// Thread-safe ring buffer — multiple producers/consumers with mutex.
//
// Whiteboard talking points:
// - Same circular-buffer indexing as SPSC, but all operations take one mutex.
// - Non-blocking push/pop: return false / nullopt when full / empty.
// - Simpler and correct for MPMC; SPSC atomics do not generalize for free.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class ThreadSafeRingBuffer {
 public:
    explicit ThreadSafeRingBuffer(size_t capacity)
        : size_(capacity + 1), buffer_(size_) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    size_t capacity() const { return size_ - 1; }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return head_ == tail_;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return next(tail_) == head_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tail_ >= head_) {
            return tail_ - head_;
        }
        return size_ - head_ + tail_;
    }

    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (next(tail_) == head_) {
            return false;
        }
        buffer_[tail_] = item;
        tail_ = next(tail_);
        return true;
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (head_ == tail_) {
            return std::nullopt;
        }
        T item = std::move(buffer_[head_]);
        head_ = next(head_);
        return item;
    }

 private:
    size_t next(size_t index) const { return (index + 1) % size_; }

    size_t size_;
    std::vector<T> buffer_;
    size_t head_ = 0;
    size_t tail_ = 0;
    mutable std::mutex mutex_;
};

bool test_thread_safe_ring_buffer() {
    ThreadSafeRingBuffer<int> ring(8);
    std::vector<int> consumed;
    std::mutex consumed_mutex;

    auto producer = [&](int start) {
        for (int i = start; i < start + 10; ++i) {
            while (!ring.push(i)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

    std::thread t1(producer, 0);
    std::thread t2(producer, 10);
    std::thread consumer([&] {
        while (static_cast<int>(consumed.size()) < 20) {
            auto item = ring.pop();
            if (item.has_value()) {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                consumed.push_back(*item);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    t1.join();
    t2.join();
    consumer.join();

    std::vector<int> sorted = consumed;
    std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < 20; ++i) {
        assert(sorted[i] == i);
    }
    return true;
}

int main() {
    assert(test_thread_safe_ring_buffer());
    std::cout << "thread_safe_ring_buffer: ok\n";
    return 0;
}
