// Problem: Fixed-size lock-free memory pool (Treiber free-list)
//
 // Design:
 //   Preallocate blockCount blocks of blockSize bytes. Free blocks form a
 //   singly linked list; allocate/free are CAS on the list head.
 //
 // Correctness notes:
 //   - After a successful CAS pop, do NOT store head again (that double-advances).
 //   - blockSize must be >= sizeof(Block); Block overlays the free-list next ptr.
 //   - ABA remains (see c++/two_level_mempool.cpp for discussion).

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>

class MemoryPool {
 public:
    struct Block {
        Block* next;
    };

    MemoryPool(std::size_t block_size, std::size_t block_count)
        : block_size_(block_size < sizeof(Block) ? sizeof(Block) : block_size),
          block_count_(block_count),
          slab_(nullptr) {
        assert(block_count_ > 0);
        slab_ = static_cast<char*>(std::malloc(block_size_ * block_count_));
        assert(slab_ != nullptr);

        for (std::size_t i = 0; i + 1 < block_count_; ++i) {
            auto* cur = block_at(i);
            cur->next = block_at(i + 1);
        }
        block_at(block_count_ - 1)->next = nullptr;
        free_list_.store(block_at(0), std::memory_order_relaxed);
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    ~MemoryPool() { std::free(slab_); }

    Block* allocate() {
        Block* block = free_list_.load(std::memory_order_acquire);
        while (block != nullptr &&
               !free_list_.compare_exchange_weak(block, block->next,
                                                 std::memory_order_acquire,
                                                 std::memory_order_acquire)) {
        }
        // CAS already swung head to block->next — return block as-is.
        return block;
    }

    void deallocate(Block* block) {
        if (block == nullptr) {
            return;
        }
        Block* old = free_list_.load(std::memory_order_relaxed);
        do {
            block->next = old;
        } while (!free_list_.compare_exchange_weak(old, block,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed));
    }

 private:
    Block* block_at(std::size_t i) {
        return reinterpret_cast<Block*>(slab_ + i * block_size_);
    }

    std::size_t block_size_;
    std::size_t block_count_;
    char* slab_;
    std::atomic<Block*> free_list_{nullptr};
};

int main() {
    MemoryPool pool(64, 4);
    auto* a = pool.allocate();
    auto* b = pool.allocate();
    assert(a && b && a != b);
    pool.deallocate(a);
    pool.deallocate(b);
    auto* c = pool.allocate();
    assert(c != nullptr);
    std::cout << "memory_pool: ok\n";
    return 0;
}
