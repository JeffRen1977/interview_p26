// SPSC ring buffer — 单生产者、单消费者无锁环形队列。
//
// Deep dive: docs/24-无锁SPSC队列与Cacheline对齐.md
//
// Whiteboard talking points:
// - 预留一个永不存放有效元素的空槽，用同一对 head/tail 区分：
//     empty: head == tail
//     full : (tail + 1) % internal_size == head
//   因此用户请求容量 N 时，内部实际分配 N+1 个槽。
// - Producer 是 tail_ 的唯一写者；Consumer 是 head_ 的唯一写者。
//   单写者约束使我们不需要 CAS，只需 atomic load/store。
// - Producer 先写 data，再 release-store tail；Consumer acquire-load tail
//   后才能读 data。反方向上，Consumer 读完 data 再 release-store head，
//   Producer acquire-load head 后才能安全复用该槽。
// - push/pop 都是 O(1)、非阻塞：满/空时立即返回 false/nullopt，
//   backpressure、sleep 或丢包策略由调用方决定。
// - head_ 和 tail_ 分别对齐 Cache Line，避免两个核心反复写同一缓存行
//   导致 False Sharing。
// - 只允许恰好一个生产线程 + 一个消费线程；MPSC/MPMC 使用本类会 data race。

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class SPSCRingBuffer {
 public:
    explicit SPSCRingBuffer(size_t capacity)
        : size_(capacity + 1) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
        // 一次性分配固定数组，此后不 resize：运行时 push/pop 不发生扩容，
        // buffer_[i] 的地址也保持稳定。
        //
        // 这个白板版用 vector<T>(N)，因此要求 T 可默认构造、可赋值。
        // 生产级泛型队列可用 aligned_storage/未初始化内存 + placement new，
        // 只在槽被占用时构造 T。
        buffer_.resize(size_);
    }

    // 对外可用容量；内部 size_ 比它多一个哨兵空槽。
    size_t capacity() const { return size_ - 1; }

    // 返回调用瞬间观察到的近似元素数。
    // 并发调用时 head/tail 是分别读取的两个快照，期间另一个线程可能推进，
    // 所以它适合监控/调试，不应拿来做严格同步条件。
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        if (tail >= head) {
            return tail - head;
        }
        // tail 已经绕回数组开头：有效区间是 [head, size_) + [0, tail)。
        return size_ - head + tail;
    }

    // 空：生产者和消费者位置相同。
    // 与 size() 一样，并发观察只代表某一瞬间，不保证下一条语句仍为空。
    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    // 满：tail 的下一个位置追上 head；预留槽保证该状态不与 empty 混淆。
    bool full() const {
        const size_t next_tail =
            (tail_.load(std::memory_order_acquire) + 1) % size_;
        return next_tail == head_.load(std::memory_order_acquire);
    }

    // Producer-only：尝试入队。成功 true，队满 false；不阻塞。
    bool push(const T& item) {
        // tail_ 只有当前生产线程写，所以读取自己的位置不需要同步，
        // relaxed 足够。
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) % size_;

        // head_ 由消费者写。acquire 与消费者 release-store head_ 配对：
        // 若看到消费者推进了 head，说明消费者已读完旧元素，该槽可复用。
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // 队满：不能覆盖尚未被消费的数据
        }

        // 只有生产者会写当前 tail 槽，消费者在 tail_ 发布前不会读取它，
        // 因此元素本身不需要 atomic。
        buffer_[tail] = item;

        // 发布新元素：release 保证上面的 buffer_ 写入不会被重排到
        // tail_ 更新之后。消费者 acquire 看到 next_tail 后必能看到 item。
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // Consumer-only：尝试出队。队空返回 nullopt，不阻塞。
    std::optional<T> pop() {
        // head_ 只有当前消费线程写，读取自己的位置用 relaxed 即可。
        const size_t head = head_.load(std::memory_order_relaxed);

        // tail_ 由生产者发布。acquire 与生产者 release-store tail_ 配对：
        // 看到 tail 前进时，也同时看到生产者此前写入 buffer_[head] 的数据。
        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;  // 队空：没有可读元素
        }

        // 移动而非拷贝，支持 vector/string 等较重对象并减少复制开销。
        // 此时该槽由消费者独占：生产者要等 head_ 发布后才能复用。
        T item = std::move(buffer_[head]);

        // 发布“消费完成”：release 保证对槽位的读取/移动发生在 head 前进前。
        // 生产者 acquire 看到新 head 后，才可覆盖这个槽。
        head_.store((head + 1) % size_, std::memory_order_release);
        return item;
    }

 private:
    // C++17 提供的“可能发生破坏性干扰”的硬件缓存行大小；
    // 常见 x86/ARM 为 64 字节。<new> 中定义。
    static constexpr size_t kCacheLine =
        std::hardware_destructive_interference_size;

    // 内部槽数 = 用户容量 + 1（哨兵空槽）。
    size_t size_;

    // 数据区域只在对应槽位所有权转移后访问，不要求每个 T 都是 atomic。
    std::vector<T> buffer_;

    // Consumer 写 head，Producer 读 head。
    // Producer 写 tail，Consumer 读 tail。
    // 分开放到不同 Cache Line，避免两核的写入让同一行来回失效。
    alignas(kCacheLine) std::atomic<size_t> head_{0};
    alignas(kCacheLine) std::atomic<size_t> tail_{0};
};

// 并发测试：一个生产者按 0..19 入队，一个消费者出队。
// SPSC 必须保持 FIFO，最终 consumed[i] == i。
bool test_spsc_ring_buffer() {
    SPSCRingBuffer<int> ring(4);
    std::vector<int> consumed;

    std::thread producer([&] {
        for (int i = 0; i < 20; ++i) {
            // 队满时 push 返回 false。测试用 sleep 降低空转；
            // 真实低延迟 Datapath 可用 pause/yield/自适应 backoff。
            while (!ring.push(i)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    std::thread consumer([&] {
        while (static_cast<int>(consumed.size()) < 20) {
            auto item = ring.pop();
            if (item.has_value()) {
                consumed.push_back(*item);
            } else {
                // 队空时让出执行资源；等待策略在队列之外。
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    // 不仅验证数量，还验证 SPSC FIFO 顺序。
    for (int i = 0; i < 20; ++i) {
        assert(consumed[i] == i);
    }
    return true;
}

int main() {
    assert(test_spsc_ring_buffer());
    std::cout << "spsc_ring_buffer: ok\n";
    return 0;
}
