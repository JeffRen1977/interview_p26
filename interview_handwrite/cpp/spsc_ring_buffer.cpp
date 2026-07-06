// SPSC ring buffer — single producer, single consumer.
//
// Deep dive: docs/24-无锁SPSC队列与Cacheline对齐.md
//
// Whiteboard talking points:
// - Reserve one empty slot so full/empty are distinguishable.
// - Producer only updates tail_; consumer only updates head_.
// - Use atomics + acquire/release so the two threads can communicate safely.
// - push/pop are O(1); non-blocking try_push/try_pop for backpressure.

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class SPSCRingBuffer {
 public:
    explicit SPSCRingBuffer(size_t capacity)
        : slots_(capacity + 1), size_(capacity + 1) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
        buffer_.resize(size_);
    }

    size_t capacity() const { return size_ - 1; }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        if (tail >= head) {
            return tail - head;
        }
        return size_ - head + tail;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const {
        const size_t next_tail =
            (tail_.load(std::memory_order_acquire) + 1) % size_;
        return next_tail == head_.load(std::memory_order_acquire);
    }

    bool push(const T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) % size_;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T item = std::move(buffer_[head]);
        head_.store((head + 1) % size_, std::memory_order_release);
        return item;
    }

 private:
    static constexpr size_t kCacheLine =
        std::hardware_destructive_interference_size;

    size_t slots_;
    size_t size_;
    std::vector<T> buffer_;
    alignas(kCacheLine) std::atomic<size_t> head_{0};
    alignas(kCacheLine) std::atomic<size_t> tail_{0};
};

bool test_spsc_ring_buffer() {
    SPSCRingBuffer<int> ring(4);
    std::vector<int> consumed;

    std::thread producer([&] {
        for (int i = 0; i < 20; ++i) {
            while (!ring.push(i)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    std::thread consumer([&] {
        while (static_cast<int>(consumed.size()) < 20) {
            auto item = ring.pop();
            if (item.has_value()) {
                consumed.push_back(*item);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    for (int i = 0; i < 20; ++i) {
        assert(consumed[i] == i);
    }
    return true;
}

int main() {
    assert(test_spsc_ring_buffer());
    std::cout << "spsc_ring_buffer: ok\n";
    return 0;
}
