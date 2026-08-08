// Fixed-size block pool — thread-safe variants (interview drill).
//
// Matches c++/固定内存大小分配.md §4 变体 2:
//   A) Lock-Free：tagged index Treiber free-list
//   B) Thread-Local：每线程本地 cache + 全局批量补给（TCMalloc 思路）
//
// Whiteboard vs production:
//   口述零开销：空闲块内嵌 Node* next。
//   多线程落地：用户写 payload 会和 CAS 读 next 打架，且纯指针 CAS 有 ABA
//   → parallel next_[] + head={index, version} 打包进 uint64 CAS。
//
// 容量在构造时固定（热路径永不 malloc）。需要扩容时面试口述：
//   慢路径 mutex 申请新 slab，把新 index 区间 splice 进 freelist。

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t kNullIdx = UINT32_MAX;

inline std::uint64_t pack_head(std::uint32_t idx, std::uint32_t tag) {
    return (static_cast<std::uint64_t>(tag) << 32) | idx;
}

inline std::uint32_t head_idx(std::uint64_t h) {
    return static_cast<std::uint32_t>(h);
}

inline std::uint32_t head_tag(std::uint64_t h) {
    return static_cast<std::uint32_t>(h >> 32);
}

}  // namespace

// =============================================================================
// 方案 A：Lock-Free FreeList（tagged index）
// =============================================================================
class LockFreeFixedBlockPool {
 public:
    // block_size: 用户 payload 大小；capacity: 固定槽位数
    LockFreeFixedBlockPool(std::size_t block_size, std::size_t capacity)
        : capacity_(static_cast<std::uint32_t>(capacity)),
          stride_(sizeof(std::uint32_t) + (block_size == 0 ? 1 : block_size)) {
        assert(capacity > 0 && capacity < kNullIdx);

        slab_ = static_cast<char*>(::operator new(stride_ * capacity_));
        next_ = new std::atomic<std::uint32_t>[capacity_];
        blocks_ = new void*[capacity_];

        // 建 freelist：0 → 1 → … → null（也可以头插，语义一样）
        for (std::uint32_t i = 0; i < capacity_; ++i) {
            char* slot = slab_ + static_cast<std::size_t>(i) * stride_;
            *reinterpret_cast<std::uint32_t*>(slot) = i;  // 永久 index 头
            blocks_[i] = slot + sizeof(std::uint32_t);
            const std::uint32_t nxt = (i + 1 < capacity_) ? (i + 1) : kNullIdx;
            next_[i].store(nxt, std::memory_order_relaxed);
        }
        head_.store(pack_head(0, 0), std::memory_order_relaxed);
    }

    LockFreeFixedBlockPool(const LockFreeFixedBlockPool&) = delete;
    LockFreeFixedBlockPool& operator=(const LockFreeFixedBlockPool&) = delete;

    ~LockFreeFixedBlockPool() {
        delete[] next_;
        delete[] blocks_;
        ::operator delete(slab_);
    }

    // 池空返回 nullptr（由调用方 backpressure / 降级）
    void* allocate() {
        std::uint64_t h = head_.load(std::memory_order_acquire);
        while (true) {
            const std::uint32_t idx = head_idx(h);
            if (idx == kNullIdx) {
                return nullptr;
            }
            const std::uint32_t nxt =
                next_[idx].load(std::memory_order_relaxed);
            const std::uint64_t nh = pack_head(nxt, head_tag(h) + 1);
            if (head_.compare_exchange_weak(h, nh, std::memory_order_acquire,
                                            std::memory_order_acquire)) {
                return blocks_[idx];
            }
        }
    }

    void deallocate(void* ptr) {
        if (ptr == nullptr) {
            return;
        }
        auto* idx_ptr = reinterpret_cast<std::uint32_t*>(
            static_cast<char*>(ptr) - sizeof(std::uint32_t));
        const std::uint32_t idx = *idx_ptr;
        assert(idx < capacity_);

        std::uint64_t h = head_.load(std::memory_order_relaxed);
        std::uint64_t nh;
        do {
            next_[idx].store(head_idx(h), std::memory_order_relaxed);
            nh = pack_head(idx, head_tag(h) + 1);
        } while (!head_.compare_exchange_weak(h, nh, std::memory_order_release,
                                              std::memory_order_relaxed));
    }

    std::uint32_t capacity() const { return capacity_; }

 private:
    std::uint32_t capacity_;
    std::size_t stride_;
    char* slab_ = nullptr;
    void** blocks_ = nullptr;                      // idx -> payload*
    std::atomic<std::uint32_t>* next_ = nullptr;   // freelist 后继
    std::atomic<std::uint64_t> head_{0};
};

// =============================================================================
// 方案 B：Thread-Local cache + 全局 Lock-Free
// =============================================================================
class TlsFixedBlockPool {
 public:
    static constexpr std::size_t kLocalBatch = 32;

    TlsFixedBlockPool(std::size_t block_size, std::size_t capacity)
        : global_(block_size, capacity) {}

    TlsFixedBlockPool(const TlsFixedBlockPool&) = delete;
    TlsFixedBlockPool& operator=(const TlsFixedBlockPool&) = delete;

    void* allocate() {
        LocalCache& local = cache_for(*this);
        if (local.size == 0 && !refill(local)) {
            return nullptr;
        }
        return local.stack[--local.size];
    }

    void deallocate(void* ptr) {
        if (ptr == nullptr) {
            return;
        }
        LocalCache& local = cache_for(*this);
        if (local.size == local.stack.size()) {
            local.stack.resize(local.stack.size() + kLocalBatch);
        }
        local.stack[local.size++] = ptr;
        if (local.size >= kLocalBatch * 2) {
            drain(local);
        }
    }

 private:
    struct LocalCache {
        std::vector<void*> stack;
        std::size_t size = 0;
        TlsFixedBlockPool* owner = nullptr;
    };

    static LocalCache& cache_for(TlsFixedBlockPool& pool) {
        thread_local LocalCache cache;
        if (cache.owner != &pool) {
            if (cache.owner != nullptr && cache.size > 0) {
                cache.owner->drain_all(cache);
            }
            cache.stack.assign(kLocalBatch * 2, nullptr);
            cache.size = 0;
            cache.owner = &pool;
        }
        return cache;
    }

    bool refill(LocalCache& local) {
        if (local.stack.size() < kLocalBatch) {
            local.stack.resize(kLocalBatch * 2);
        }
        for (std::size_t i = 0; i < kLocalBatch; ++i) {
            void* p = global_.allocate();
            if (p == nullptr) {
                return local.size > 0;
            }
            local.stack[local.size++] = p;
        }
        return true;
    }

    void drain(LocalCache& local) {
        while (local.size > kLocalBatch) {
            global_.deallocate(local.stack[--local.size]);
        }
    }

    void drain_all(LocalCache& local) {
        while (local.size > 0) {
            global_.deallocate(local.stack[--local.size]);
        }
    }

    LockFreeFixedBlockPool global_;
};

// =============================================================================
// Tests
// =============================================================================

void test_lockfree_basic() {
    LockFreeFixedBlockPool pool(16, 8);
    void* a = pool.allocate();
    void* b = pool.allocate();
    assert(a && b && a != b);
    *static_cast<int*>(a) = 1;
    *static_cast<int*>(b) = 2;
    pool.deallocate(a);
    void* c = pool.allocate();
    assert(c == a);  // LIFO
    pool.deallocate(b);
    pool.deallocate(c);
}

void test_lockfree_exhaust() {
    LockFreeFixedBlockPool pool(8, 4);
    void* ps[4];
    for (auto& p : ps) {
        p = pool.allocate();
        assert(p);
    }
    assert(pool.allocate() == nullptr);
    for (auto* p : ps) {
        pool.deallocate(p);
    }
    assert(pool.allocate() != nullptr);
}

void test_lockfree_concurrent() {
    constexpr int kCap = 64;
    constexpr int kThreads = 4;
    constexpr int kOps = 8000;
    LockFreeFixedBlockPool pool(32, kCap);
    std::atomic<int> done{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kOps; ++i) {
                void* p = nullptr;
                while ((p = pool.allocate()) == nullptr) {
                    std::this_thread::yield();
                }
                *static_cast<int*>(p) = i;
                pool.deallocate(p);
                done.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    assert(done.load() == kThreads * kOps);
}

void test_tls_concurrent() {
    constexpr int kCap = 256;
    constexpr int kThreads = 4;
    constexpr int kOps = 8000;
    TlsFixedBlockPool pool(32, kCap);
    std::atomic<int> done{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kOps; ++i) {
                void* p = nullptr;
                while ((p = pool.allocate()) == nullptr) {
                    std::this_thread::yield();
                }
                *static_cast<int*>(p) = i;
                pool.deallocate(p);
                done.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    assert(done.load() == kThreads * kOps);
}

int main() {
    test_lockfree_basic();
    test_lockfree_exhaust();
    test_lockfree_concurrent();
    test_tls_concurrent();
    std::cout << "fixed_block_pool_thread_safe: ok\n";
    return 0;
}
