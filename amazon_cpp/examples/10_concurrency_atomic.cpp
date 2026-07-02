// Part 13-15: mutex, condition_variable, atomic, false sharing.

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

// Bad: counters may share a cache line.
struct BadCounters {
    std::atomic<int> a{0};
    std::atomic<int> b{0};
};

// Better: pad to separate cache lines (typically 64 bytes).
struct alignas(64) PaddedCounter {
    std::atomic<int> value{0};
    char pad[64 - sizeof(std::atomic<int>)];
};

void producer_consumer() {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<int> q;
    bool done = false;

    std::thread producer([&] {
        for (int i = 0; i < 5; ++i) {
            {
                std::lock_guard lock(mtx);
                q.push(i);
            }
            cv.notify_one();
        }
        {
            std::lock_guard lock(mtx);
            done = true;
        }
        cv.notify_one();
    });

    std::thread consumer([&] {
        std::unique_lock lock(mtx);
        int sum = 0;
        while (true) {
            cv.wait(lock, [&] { return done || !q.empty(); });
            while (!q.empty()) {
                sum += q.front();
                q.pop();
            }
            if (done) break;
        }
        assert(sum == 0 + 1 + 2 + 3 + 4);
    });

    producer.join();
    consumer.join();
    std::cout << "producer-consumer: ok\n";
}

void atomic_acquire_release() {
    std::atomic<bool> ready{false};
    int data = 0;

    std::thread t1([&] {
        data = 42;
        ready.store(true, std::memory_order_release);
    });
    std::thread t2([&] {
        while (!ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        assert(data == 42);
    });
    t1.join();
    t2.join();
    std::cout << "acquire-release: ok\n";
}

void cas_demo() {
    std::atomic<int> counter{0};
    int expected = 0;
    assert(counter.compare_exchange_strong(expected, 1));
    assert(counter.load() == 1);
    expected = 0;
    assert(!counter.compare_exchange_strong(expected, 2));
    assert(expected == 1);
    std::cout << "CAS: ok\n";
}

int main() {
    producer_consumer();
    atomic_acquire_release();
    cas_demo();
    std::cout << "sizeof(BadCounters)=" << sizeof(BadCounters)
              << " sizeof(PaddedCounter)=" << sizeof(PaddedCounter) << "\n";
    std::cout << "10_concurrency_atomic: ok\n";
    return 0;
}
