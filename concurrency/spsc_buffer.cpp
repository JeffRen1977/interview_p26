// Problem: Lock-free SPSC ring buffer (minimal whiteboard)
//
// Scenario:
 //   One camera capture thread produces; one analytics thread consumes.
 //   Exactly one producer + one consumer → no mutex / no CAS.
 //
 // Design:
 //   Circular array with one sentinel empty slot so empty/full are distinct:
 //     empty: head == tail
 //     full : (tail + 1) % N == head
 //   User capacity C ⇒ internal size C+1.
 //
 // Memory orders:
 //   Producer: write slot, then release-store tail.
 //   Consumer: acquire-load tail, read slot, then release-store head.
 //
 // For a fuller version (cache-line padding, tests): see spsc_ring_buffer.cpp.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

template <typename T>
class SPSCBuffer {
 public:
    explicit SPSCBuffer(std::size_t capacity)
        : size_(capacity + 1), buffer_(size_) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    std::size_t capacity() const { return size_ - 1; }

    bool push(const T& value) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) % size_;
        if (next == head_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[tail] = value;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;  // empty
        }
        T value = buffer_[head];
        head_.store((head + 1) % size_, std::memory_order_release);
        return value;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const {
        const std::size_t next =
            (tail_.load(std::memory_order_acquire) + 1) % size_;
        return next == head_.load(std::memory_order_acquire);
    }

 private:
    const std::size_t size_;
    std::vector<T> buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

int main() {
    SPSCBuffer<int> q(2);
    assert(q.capacity() == 2);
    assert(q.empty());
    assert(q.push(1));
    assert(q.push(2));
    assert(!q.push(3));  // full
    assert(q.full());
    assert(q.pop() == 1);
    assert(q.push(3));
    assert(q.pop() == 2);
    assert(q.pop() == 3);
    assert(!q.pop().has_value());
    std::cout << "spsc_buffer: ok\n";
    return 0;
}
