// Fixed-block frame-buffer pool: O(1) alloc/free, no fragmentation, no malloc
// in the capture path. The allocator every camera driver ships.
//
// Two layers:
//   1. FramedPool      — intrusive free list over one aligned slab.
//   2. RefCountedPool  — buffers with a refcount, because one frame is shared
//                        by preview + encoder + CV and must return to the pool
//                        only when the last consumer drops it.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Single-threaded fixed-block pool.
//
// The free list lives *inside* the free blocks: a free block's first bytes
// hold the pointer to the next free block. Zero metadata overhead, which is
// why this beats a bitmap when blocks are large.
// ---------------------------------------------------------------------------

class FramePool {
public:
    // block_size is rounded up to `alignment` so every block stays aligned
    // (DMA and SIMD both care). alignment must be a power of two >= sizeof(void*).
    FramePool(size_t block_size, size_t block_count, size_t alignment = 64)
        : block_size_(round_up(block_size < sizeof(void*) ? sizeof(void*) : block_size,
                               alignment)),
          count_(block_count) {
        assert((alignment & (alignment - 1)) == 0 && "alignment must be a power of two");
        assert(alignment >= sizeof(void*));
        slab_ = static_cast<uint8_t*>(aligned_alloc_compat(alignment, block_size_ * count_));
        assert(slab_ != nullptr);
        build_free_list();
    }

    ~FramePool() { std::free(slab_); }

    FramePool(const FramePool&) = delete;
    FramePool& operator=(const FramePool&) = delete;

    void* acquire() {
        if (!free_head_) return nullptr;             // exhausted: caller must drop a frame
        void* blk = free_head_;
        free_head_ = *reinterpret_cast<void**>(free_head_);
        ++in_use_;
        return blk;
    }

    void release(void* p) {
        if (!p) return;
        assert(owns(p) && "released a pointer this pool never handed out");
        assert(in_use_ > 0);
        *reinterpret_cast<void**>(p) = free_head_;
        free_head_ = p;
        --in_use_;
    }

    bool owns(const void* p) const {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        if (b < slab_ || b >= slab_ + block_size_ * count_) return false;
        return static_cast<size_t>(b - slab_) % block_size_ == 0;   // and on a block boundary
    }

    // Index of a block this pool handed out — lets a caller keep a parallel
    // metadata array without storing a back-pointer in every block.
    size_t index_of(const void* p) const {
        assert(owns(p));
        return static_cast<size_t>(static_cast<const uint8_t*>(p) - slab_) / block_size_;
    }

    size_t block_size() const { return block_size_; }
    size_t capacity() const { return count_; }
    size_t in_use() const { return in_use_; }
    size_t available() const { return count_ - in_use_; }

private:
    static size_t round_up(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

    static void* aligned_alloc_compat(size_t alignment, size_t size) {
        void* p = nullptr;
        if (posix_memalign(&p, alignment, size) != 0) return nullptr;
        return p;
    }

    void build_free_list() {
        free_head_ = nullptr;
        // Link backwards so acquire() returns blocks in ascending address order,
        // which makes test output and memory dumps readable.
        for (size_t i = count_; i-- > 0;) {
            void* blk = slab_ + i * block_size_;
            *reinterpret_cast<void**>(blk) = free_head_;
            free_head_ = blk;
        }
        in_use_ = 0;
    }

    size_t block_size_, count_;
    uint8_t* slab_ = nullptr;
    void* free_head_ = nullptr;
    size_t in_use_ = 0;
};

// ---------------------------------------------------------------------------
// 2. Ref-counted buffers shared across consumers.
//
// The refcount is atomic; the free list is behind a mutex. That split is
// deliberate: the hot path (a consumer dropping its reference) is a single
// atomic decrement, and only the last one pays for the lock.
// ---------------------------------------------------------------------------

class RefCountedPool;

struct FrameBuffer {
    void* data;
    size_t size;
    uint32_t seq;
    int64_t timestamp_ns;
    std::atomic<int> refs;
    RefCountedPool* owner;
};

class RefCountedPool {
public:
    RefCountedPool(size_t block_size, size_t block_count)
        : pool_(block_size, block_count), meta_(block_count) {
        for (size_t i = 0; i < block_count; ++i) meta_[i].owner = this;
    }

    // Hands back a buffer with refcount 1.
    FrameBuffer* acquire(uint32_t seq, int64_t ts) {
        std::lock_guard<std::mutex> lk(m_);
        void* blk = pool_.acquire();
        if (!blk) return nullptr;
        const size_t idx = pool_.index_of(blk);
        FrameBuffer* fb = &meta_[idx];
        fb->data = blk;
        fb->size = pool_.block_size();
        fb->seq = seq;
        fb->timestamp_ns = ts;
        fb->refs.store(1, std::memory_order_relaxed);
        return fb;
    }

    // A consumer that will outlive the producer's reference takes its own.
    static void add_ref(FrameBuffer* fb) {
        fb->refs.fetch_add(1, std::memory_order_relaxed);
    }

    static void release(FrameBuffer* fb) {
        // release ordering so writes to the buffer happen-before the recycle;
        // acquire fence on the last drop so the recycler sees them.
        if (fb->refs.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            fb->owner->recycle(fb);
        }
    }

    size_t available() {
        std::lock_guard<std::mutex> lk(m_);
        return pool_.available();
    }

private:
    void recycle(FrameBuffer* fb) {
        std::lock_guard<std::mutex> lk(m_);
        pool_.release(fb->data);
        fb->data = nullptr;
    }

    std::mutex m_;
    FramePool pool_;
    std::vector<FrameBuffer> meta_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_basic_acquire_release() {
    FramePool pool(100, 4, 64);
    assert(pool.block_size() == 128);          // 100 rounded up to 64-alignment
    assert(pool.capacity() == 4 && pool.available() == 4);

    void* a = pool.acquire();
    void* b = pool.acquire();
    assert(a && b && a != b);
    assert(pool.in_use() == 2);

    pool.release(a);
    assert(pool.in_use() == 1);
    void* c = pool.acquire();
    assert(c == a);                            // LIFO: freshest block is hottest in cache
    pool.release(b);
    pool.release(c);
    assert(pool.in_use() == 0 && pool.available() == 4);
}

static void test_exhaustion_returns_null_not_crash() {
    FramePool pool(64, 3);
    void* p[3];
    for (int i = 0; i < 3; ++i) { p[i] = pool.acquire(); assert(p[i]); }
    assert(pool.acquire() == nullptr);         // out of buffers -> caller drops a frame
    pool.release(p[1]);
    void* q = pool.acquire();
    assert(q == p[1]);
    pool.release(p[0]); pool.release(p[2]); pool.release(q);
}

static void test_alignment_and_no_overlap() {
    const size_t bs = 300, n = 8, al = 64;
    FramePool pool(bs, n, al);
    std::vector<uint8_t*> blocks;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* p = static_cast<uint8_t*>(pool.acquire());
        assert(p);
        assert(reinterpret_cast<uintptr_t>(p) % al == 0);
        blocks.push_back(p);
    }
    // Write a signature into every block, then verify no block clobbered another.
    for (size_t i = 0; i < n; ++i) std::memset(blocks[i], static_cast<int>(i + 1), bs);
    for (size_t i = 0; i < n; ++i)
        for (size_t k = 0; k < bs; ++k)
            assert(blocks[i][k] == static_cast<uint8_t>(i + 1));
    for (uint8_t* p : blocks) pool.release(p);
}

static void test_owns() {
    FramePool pool(64, 2);
    void* a = pool.acquire();
    int stack_var = 0;
    assert(pool.owns(a));
    assert(!pool.owns(&stack_var));
    pool.release(a);
}

static void test_refcount_last_drop_recycles() {
    RefCountedPool pool(256, 2);
    FrameBuffer* fb = pool.acquire(1, 1000);
    assert(fb && pool.available() == 1);

    RefCountedPool::add_ref(fb);       // encoder takes a reference
    RefCountedPool::add_ref(fb);       // CV takes a reference
    RefCountedPool::release(fb);       // producer drops
    RefCountedPool::release(fb);       // encoder drops
    assert(pool.available() == 1);     // still held by CV
    RefCountedPool::release(fb);       // CV drops -> recycled
    assert(pool.available() == 2);
}

static void test_refcount_concurrent_drops() {
    RefCountedPool pool(256, 1);
    FrameBuffer* fb = pool.acquire(1, 0);
    assert(fb);
    const int N = 8;
    for (int i = 0; i < N; ++i) RefCountedPool::add_ref(fb);   // refs = 1 + N

    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i)
        ts.emplace_back([fb] { RefCountedPool::release(fb); });
    for (auto& t : ts) t.join();
    assert(pool.available() == 0);      // producer's reference still outstanding
    RefCountedPool::release(fb);
    assert(pool.available() == 1);      // exactly one recycle happened
}

int main() {
    test_basic_acquire_release();
    test_exhaustion_returns_null_not_crash();
    test_alignment_and_no_overlap();
    test_owns();
    test_refcount_last_drop_recycles();
    test_refcount_concurrent_drops();
    std::cout << "frame_pool: ok\n";
    return 0;
}
