// Problem: Thread-safe bounded queue (mutex + two condition variables)
//
 // Scenario:
 //   Multiple producers / consumers share a fixed-capacity queue.
 //   push blocks when full; pop blocks when empty.
 //
 // Design:
 //   One mutex + not_empty / not_full CVs. Always wait with a predicate
 //   (while/lambda), never a bare if.
 //
 // Prefer bounded_blocking_queue.cpp for timeouts, try_put/try_get, and tests.
 // This file is the minimal whiteboard skeleton.

#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>

template <typename T>
class ThreadSafeQueue {
 public:
    explicit ThreadSafeQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(std::move(value));
        not_empty_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size() == capacity_;
    }

 private:
    const std::size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

int main() {
    ThreadSafeQueue<int> q(2);
    q.push(1);
    q.push(2);
    assert(q.full());
    assert(q.pop() == 1);
    assert(q.pop() == 2);
    assert(q.empty());
    std::cout << "thread_safe_queue: ok\n";
    return 0;
}
