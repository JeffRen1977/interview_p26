// Lock-free MPMC ring buffer — per-slot sequence + CAS.
//
// Deep dive: docs/25-无锁MPMC队列与CAS.md
//
// Whiteboard talking points:
// - Capacity must be power of 2; use pos & mask instead of %.
// - Each slot has atomic sequence: empty when seq==pos, ready when seq==pos+1.
// - enqueue/dequeue CAS global positions; weak CAS in retry loops.
// - Store descriptors (e.g. dma iova), not large tensor payloads.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#if defined(__x86_64__)
#include <immintrin.h>
#elif defined(__aarch64__)
#endif

template <typename T>
struct alignas(64) MPMCNode {
    T data{};
    std::atomic<size_t> sequence{0};
};

template <typename T>
class LockFreeMPMCQueue {
 public:
    explicit LockFreeMPMCQueue(size_t capacity)
        : capacity_(capacity), buffer_mask_(capacity - 1) {
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("capacity must be a power of 2");
        }
        buffer_ = new MPMCNode<T>[capacity_];
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~LockFreeMPMCQueue() { delete[] buffer_; }

    LockFreeMPMCQueue(const LockFreeMPMCQueue&) = delete;
    LockFreeMPMCQueue& operator=(const LockFreeMPMCQueue&) = delete;

    bool enqueue(const T& data) {
        MPMCNode<T>* node = nullptr;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        int retries = 0;

        while (true) {
            node = &buffer_[pos & buffer_mask_];
            const size_t seq = node->sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) -
                                  static_cast<intptr_t>(pos);

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(
                        pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
                backoff(retries++);
            } else if (diff < 0) {
                return false;
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
                retries = 0;
            }
        }

        node->data = data;
        node->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& data) {
        MPMCNode<T>* node = nullptr;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        int retries = 0;

        while (true) {
            node = &buffer_[pos & buffer_mask_];
            const size_t seq = node->sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) -
                                  static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(
                        pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
                backoff(retries++);
            } else if (diff < 0) {
                return false;
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
                retries = 0;
            }
        }

        data = node->data;
        node->sequence.store(pos + capacity_, std::memory_order_release);
        return true;
    }

 private:
    static void backoff(int retries) {
        const int limit = retries > 10 ? 10 : retries;
        for (int i = 0; i < (1 << limit); ++i) {
#if defined(__x86_64__)
            _mm_pause();
#elif defined(__aarch64__)
            __asm__ __volatile__("yield" ::: "memory");
#else
            (void)0;
#endif
        }
    }

    const size_t capacity_;
    const size_t buffer_mask_;
    MPMCNode<T>* buffer_ = nullptr;

    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};
};

struct TensorDesc {
    uint64_t dma_iova = 0;
    uint32_t byte_len = 0;
    uint16_t task_id = 0;
};

bool test_mpmc_ring_buffer() {
    LockFreeMPMCQueue<uint64_t> queue(16);
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 250;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<int> produced;
    std::vector<int> consumed;
    std::mutex produced_mutex;
    std::mutex consumed_mutex;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                const uint64_t value =
                    (static_cast<uint64_t>(p) << 32) |
                    static_cast<uint64_t>(i);
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
                std::lock_guard lock(produced_mutex);
                produced.push_back(static_cast<int>(value));
            }
        });
    }

    std::atomic<int> consumed_count{0};

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (consumed_count.load(std::memory_order_acquire) < kTotal) {
                uint64_t value = 0;
                if (queue.dequeue(value)) {
                    std::lock_guard lock(consumed_mutex);
                    consumed.push_back(static_cast<int>(value));
                    consumed_count.fetch_add(1, std::memory_order_release);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    assert(static_cast<int>(produced.size()) == kTotal);
    assert(static_cast<int>(consumed.size()) == kTotal);

    std::sort(produced.begin(), produced.end());
    std::sort(consumed.begin(), consumed.end());
    assert(produced == consumed);
    return true;
}

int main() {
    assert(test_mpmc_ring_buffer());
    std::cout << "mpmc_ring_buffer: ok\n";
    return 0;
}
