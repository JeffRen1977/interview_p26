// Thread-safe bounded blocking queue (producer-consumer).
//
// Whiteboard talking points:
// - One mutex protects the deque.
// - Two condition variables: not_empty / not_full.
// - put() blocks when full; get() blocks when empty.
// - Always use while (not if) when waiting on a condition variable.
// - Amortized O(1) for put/get.

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class BoundedBlockingQueue {
 public:
    explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void put(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push_back(item);
        not_empty_.notify_one();
    }

    bool put(const T& item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!not_full_.wait_for(lock, timeout,
                                [this] { return queue_.size() < capacity_; })) {
            return false;
        }
        queue_.push_back(item);
        not_empty_.notify_one();
        return true;
    }

    T get() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

    std::optional<T> get(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!not_empty_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

    bool try_put(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(item);
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> try_get() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

 private:
    size_t capacity_;
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

bool test_bounded_blocking_queue() {
    BoundedBlockingQueue<int> queue(2);
    std::vector<int> produced;
    std::vector<int> consumed;

    std::thread producer([&] {
        for (int i = 0; i < 5; ++i) {
            queue.put(i);
            produced.push_back(i);
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < 5; ++i) {
            consumed.push_back(queue.get());
        }
    });

    producer.join();
    consumer.join();

    for (int i = 0; i < 5; ++i) {
        assert(produced[i] == i);
        assert(consumed[i] == i);
    }
    return true;
}

int main() {
    assert(test_bounded_blocking_queue());
    std::cout << "bounded_blocking_queue: ok\n";
    return 0;
}
