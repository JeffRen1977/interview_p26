// Two-level mempool: Global Treiber free-list + Per-core Local Cache.
//
// Global layer matches the classic FixedSizeMemoryPool whiteboard:
//   raw contiguous slab → free list of Node* → CAS pop/push.
// Local layer avoids hitting that CAS on every packet:
//   thread-private array; refill/drain in bulk when empty/full.
//
// Follow-ups:
// - ABA on free-list head (see allocate() comment) → tagged pointer / versioned CAS
// - Production bulk = one CAS cutting a chain (DPDK), not a loop of singles
// - Hugepage mmap for the slab; RSS so each core owns one LocalCache

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

constexpr std::size_t kCacheLine = 64;
constexpr std::size_t kLocalSize = 32;
constexpr std::size_t kBulk = kLocalSize / 2;

// Free: next pointer. In use: payload. Same storage via union.
struct alignas(kCacheLine) Block {
    union {
        Block* next;
        std::uint8_t data[2048];
    };
};

// ---------------------------------------------------------------------------
// Global = FixedSizeMemoryPool style Treiber free-list (shared, CAS).
// ---------------------------------------------------------------------------
class GlobalMempool {
 public:
    GlobalMempool(const GlobalMempool&) = delete;
    GlobalMempool& operator=(const GlobalMempool&) = delete;

    // Caller owns the slab (hugepage / aligned buffer). Pool never frees it.
    GlobalMempool(void* raw_memory, std::size_t bytes) {
        const std::size_t n = bytes / sizeof(Block);
        auto* blocks = static_cast<Block*>(raw_memory);

        // Link slab into one free list: 0 → 1 → … → nullptr
        for (std::size_t i = 0; i + 1 < n; ++i) {
            blocks[i].next = &blocks[i + 1];
        }
        if (n > 0) {
            blocks[n - 1].next = nullptr;
            head_.store(&blocks[0], std::memory_order_relaxed);
        }
    }

    // Lock-free pop (same pattern as FixedSizeMemoryPool::Allocate).
    //
    // Interview Q: does this CAS free-list have an ABA problem?
    //
    // A: Yes. CAS only checks that head_ still equals the pointer we saw (X).
    //    It cannot tell that X was popped, mutated, and pushed back meanwhile.
    //
    //    Classic sequence (Treiber stack):
    //      head: X → Y → Z
    //      Thread A: loads head=X, reads X->next=Y, then pauses before CAS
    //      Thread B: allocate() pops X, then pops Y; later deallocate(X)
    //                so head is again X, but X->next may now be Z (or anything)
    //      Thread A: CAS(head, X → Y) SUCCEEDS  (head still looks like X!)
    //                → free list wrongly points at Y, which may be in use /
    //                  no longer the true successor → lost nodes / corruption
    //
    //    Local cache reduces how often we hit this path, but does NOT remove ABA
    //    on the global head_ when cores do burst refill/drain.
    //
    // Production fix — tagged / versioned head:
    //   pack {ptr, version} into 128-bit (or pointer + ABA tag) and CAS both.
    //   Every successful pop/push bumps version, so A→B→A no longer matches.
    //   Alternatives: hazard pointers / epoch reclamation if nodes can be freed
    //   to the OS (less common for a fixed slab pool).
    Block* allocate() {
        Block* old = head_.load(std::memory_order_acquire);
        // desired = old->next: if another thread wins, old is refreshed and we retry.
        while (old != nullptr &&
               !head_.compare_exchange_weak(old, old->next,
                                            std::memory_order_acquire,
                                            std::memory_order_acquire)) {
        }
        return old;  // nullptr if exhausted
    }

    // Lock-free push (same pattern as FixedSizeMemoryPool::Deallocate).
    // Pushing to the head is what enables the A→B→A pattern above: the same
    // block address can reappear as head_ after other nodes were popped.
    void deallocate(Block* block) {
        if (block == nullptr) {
            return;
        }
        Block* old = head_.load(std::memory_order_relaxed);
        do {
            block->next = old;
        } while (!head_.compare_exchange_weak(old, block,
                                              std::memory_order_release,
                                              std::memory_order_relaxed));
    }

    // Whiteboard bulk = loop of single CAS. Production: splice a chain once.
    std::size_t get_bulk(Block** out, std::size_t n) {
        std::size_t got = 0;
        while (got < n) {
            Block* b = allocate();
            if (b == nullptr) {
                break;
            }
            out[got++] = b;
        }
        return got;
    }

    void put_bulk(Block** in, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            deallocate(in[i]);
        }
    }

 private:
    // Contended CAS target — also the ABA surface (pointer identity only).
    alignas(kCacheLine) std::atomic<Block*> head_{nullptr};
};

// ---------------------------------------------------------------------------
// Local = private stack. Fast path: no atomics. Slow path: bulk ↔ global.
// ---------------------------------------------------------------------------
class alignas(kCacheLine) LocalMempoolCache {
 public:
    explicit LocalMempoolCache(GlobalMempool& global) : global_(global) {}

    void* allocate() {
        if (len_ == 0) {
            len_ = global_.get_bulk(cache_, kBulk);  // refill half cache
            if (len_ == 0) {
                return nullptr;
            }
        }
        return cache_[--len_];
    }

    void deallocate(void* ptr) {
        if (ptr == nullptr) {
            return;
        }
        if (len_ == kLocalSize) {
            // Drain half back so we keep a working set locally.
            global_.put_bulk(&cache_[kBulk], kBulk);
            len_ = kBulk;
        }
        cache_[len_++] = static_cast<Block*>(ptr);
    }

 private:
    GlobalMempool& global_;
    Block* cache_[kLocalSize]{};
    std::size_t len_ = 0;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_basic() {
    alignas(kCacheLine) std::array<Block, 64> slab{};
    GlobalMempool global(slab.data(), sizeof(slab));
    LocalMempoolCache local(global);

    void* p = local.allocate();
    assert(p != nullptr);
    static_cast<std::uint8_t*>(p)[0] = 42;
    local.deallocate(p);
}

void test_exhaust_and_recycle() {
    alignas(kCacheLine) std::array<Block, 20> slab{};
    GlobalMempool global(slab.data(), sizeof(slab));
    LocalMempoolCache local(global);

    std::vector<void*> held;
    while (void* p = local.allocate()) {
        held.push_back(p);
    }
    assert(held.size() == 20);
    assert(local.allocate() == nullptr);

    for (void* p : held) {
        local.deallocate(p);
    }
    assert(local.allocate() != nullptr);
}

void test_multi_core() {
    alignas(kCacheLine) std::array<Block, 256> slab{};
    GlobalMempool global(slab.data(), sizeof(slab));
    std::atomic<int> done{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            LocalMempoolCache local(global);  // one cache per core/thread
            for (int i = 0; i < 2000; ++i) {
                void* p = nullptr;
                while ((p = local.allocate()) == nullptr) {
                    std::this_thread::yield();
                }
                local.deallocate(p);
                done.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    assert(done.load() == 8000);
}

int main() {
    test_basic();
    test_exhaust_and_recycle();
    test_multi_core();
    std::cout << "two_level_mempool: ok\n";
    return 0;
}
