// Interview whiteboard: lock-free fixed-size memory pool.
//
// Four ideas to remember:
// 1. Caller gives one contiguous buffer → hot path never calls malloc/new.
// 2. Free slots form a singly linked list; allocate/deallocate use CAS on head.
// 3. Placement new / explicit destructor manage object lifetime without heap.
// 4. Each slot is 64-byte aligned to reduce false sharing.
//
// Follow-ups for the interviewer (see docs/09):
// - This Treiber free list has ABA; production uses tagged pointers / indexes.
// - Contended global head → Per-Core Cache (DPDK-style).

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <thread>
#include <utility>
#include <vector>

constexpr std::size_t kCacheLineSize = 64;

template <typename T>
class FixedSizeMemoryPool {
 public:
    FixedSizeMemoryPool(const FixedSizeMemoryPool&) = delete;
    FixedSizeMemoryPool& operator=(const FixedSizeMemoryPool&) = delete;

    // raw_memory: preallocated contiguous buffer (e.g. hugepage / aligned array).
    FixedSizeMemoryPool(void* raw_memory, std::size_t memory_size) {
        // Round block size up to a multiple of 64 so adjacent slots do not
        // share a cache line when different cores write different objects.
        const std::size_t need =
            sizeof(Node) > sizeof(T) ? sizeof(Node) : sizeof(T);
        block_size_ =
            ((need + kCacheLineSize - 1) / kCacheLineSize) * kCacheLineSize;

        capacity_ = memory_size / block_size_;
        auto* bytes = static_cast<std::uint8_t*>(raw_memory);

        // Build free list: slot0 → slot1 → ... → nullptr
        Node* head = nullptr;
        for (std::size_t i = 0; i < capacity_; ++i) {
            auto* node = reinterpret_cast<Node*>(bytes + i * block_size_);
            node->next = head;
            head = node;
        }
        head_.store(head, std::memory_order_relaxed);
    }

    // Pop one free slot with CAS, then construct T in place.
    template <typename... Args>
    T* allocate(Args&&... args) {
        Node* old_head = head_.load(std::memory_order_acquire);

        // CAS: head = head->next  (lock-free pop)
        //
        // compare_exchange_weak(expected, desired, success_order, failure_order)
        // 在这里各参数的含义是：
        //   expected = old_head       我之前观察到的链表头
        //   desired  = old_head->next 如果头没有变化，希望将它弹出
        //
        // 成功：head_ 仍等于 old_head，CAS 原子地把 head_ 改成 next，
        //       当前线程因此独占 old_head 指向的槽位。
        //
        // 失败：说明另一个线程抢先修改了 head_（也可能是 weak 伪失败）。
        //       CAS 会自动把 head_ 的最新值写回 old_head，循环下一轮
        //       会根据新的 old_head 重新计算 old_head->next。
        //
        // old_head != nullptr 必须先判断，否则计算 old_head->next 会解引用空指针。
        while (old_head != nullptr &&
               !head_.compare_exchange_weak(
                   old_head, old_head->next, std::memory_order_release,
                   std::memory_order_acquire)) {
            // 循环体可以为空：CAS 失败时已经替我们刷新了 old_head。
        }

        // acquire：成功取得槽位后，能看到之前 deallocate() 通过 release
        // 发布的 next/对象生命周期操作。Pop 本身不向其他线程发布数据，
        // 所以成功序不需要 release。
        //
        // 面试追问：这个白板版仍有 ABA 与 Node/T 复用同一内存的生命周期
        // 风险；生产版本应使用 tagged {version,index} 和永久 metadata。

        if (old_head == nullptr) {
            return nullptr;  // pool empty → drop / backpressure in datapath
        }

        // No malloc: only start T's lifetime at this address.
        return ::new (static_cast<void*>(old_head))
            T(std::forward<Args>(args)...);
    }

    // Destroy T, then push the slot back onto the free list with CAS.
    void deallocate(T* ptr) {
        if (ptr == nullptr) {
            return;
        }

        ptr->~T();  // never call delete — storage belongs to the pool

        auto* node = reinterpret_cast<Node*>(ptr);
        Node* old_head = head_.load(std::memory_order_relaxed);

        // CAS: node->next = head; head = node  (lock-free push)
        do {
            node->next = old_head;
        } while (!head_.compare_exchange_weak(
            old_head, node, std::memory_order_release,
            std::memory_order_relaxed));
    }

    std::size_t capacity() const { return capacity_; }
    std::size_t block_size() const { return block_size_; }

 private:
    // Free slot reuses its own storage as a linked-list node.
    // (Interview note: overlapping Node and T is the classic whiteboard model;
    //  production separates permanent free-list metadata from T — see docs/09.)
    struct Node {
        Node* next;
    };

    std::atomic<Node*> head_{nullptr};
    std::size_t block_size_ = 0;
    std::size_t capacity_ = 0;
};

struct PacketDesc {
    std::uint64_t dma = 0;
    std::uint32_t len = 0;
    std::uint32_t qid = 0;

    PacketDesc(std::uint64_t d, std::uint32_t l, std::uint32_t q) noexcept
        : dma(d), len(l), qid(q) {}
};

void test_basic() {
    alignas(kCacheLineSize) std::array<std::byte, kCacheLineSize * 4> mem{};
    FixedSizeMemoryPool<PacketDesc> pool(mem.data(), mem.size());

    assert(pool.capacity() == 4);
    assert(pool.block_size() == kCacheLineSize);

    auto* a = pool.allocate(1, 64, 0);
    auto* b = pool.allocate(2, 64, 1);
    assert(a && b);
    pool.deallocate(a);

    auto* c = pool.allocate(3, 128, 2);
    assert(c == a);  // reused the same slot

    pool.deallocate(b);
    pool.deallocate(c);
}

void test_concurrent() {
    constexpr int kSlots = 32;
    constexpr int kThreads = 4;
    constexpr int kOps = 2000;

    alignas(kCacheLineSize)
        std::array<std::byte, kCacheLineSize * kSlots> mem{};
    FixedSizeMemoryPool<PacketDesc> pool(mem.data(), mem.size());
    std::atomic<int> done{0};
    std::vector<std::thread> threads;

    for (int tid = 0; tid < kThreads; ++tid) {
        threads.emplace_back([&, tid] {
            for (int i = 0; i < kOps; ++i) {
                PacketDesc* p = nullptr;
                while ((p = pool.allocate(i, 64, tid)) == nullptr) {
                    std::this_thread::yield();
                }
                pool.deallocate(p);
                done.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(done.load() == kThreads * kOps);
}

int main() {
    test_basic();
    test_concurrent();
    std::cout << "11_lock_free_fixed_pool: ok\n";
    return 0;
}
