// Lock-free MPMC ring buffer — per-slot sequence + CAS（Dmitry Vyukov 经典算法）。
//
// Deep dive: docs/25-无锁MPMC队列与CAS.md
//
// Whiteboard talking points:
// - 容量必须是 2 的幂：用 pos & mask 代替 pos % capacity（省除法指令）。
// - 核心难点：MPMC 下"抢到槽位"和"写完数据"是两个时刻，中间别的线程
//   不能读到半成品。解法是给每个槽配一个原子 sequence，充当槽位状态机：
//     seq == pos          → 槽空，生产者可写
//     seq == pos + 1      → 数据就绪，消费者可读
//     seq == pos + cap    → 槽已腾空，等下一圈的生产者
//   生产者/消费者先 CAS 抢全局位置（占坑），再慢慢读写数据，
//   最后用 release store 更新 sequence 宣布"这个槽可以进入下一状态了"。
// - CAS 用 weak 版本：本来就在 while 重试循环里，允许 spurious failure，
//   在 ARM（LL/SC 架构）上比 strong 便宜。
// - 内存序：sequence 的 load 用 acquire、store 用 release，构成
//   synchronizes-with 配对，保证数据读写不会越过状态发布；
//   全局位置 enqueue_pos_/dequeue_pos_ 只是"分票号"，用 relaxed 即可。
// - 失败快返回（队满/队空返回 false）而不是阻塞：无锁结构里等待策略
//   交给调用方（yield / backoff / 降级走锁）。
// - 实践建议：槽里存描述符（如 dma iova + 长度），不要存大 tensor 本体，
//   否则拷贝开销和 false sharing 会吃掉无锁带来的收益。

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
#include <immintrin.h>  // _mm_pause()
#elif defined(__aarch64__)
// aarch64 直接用内联汇编 yield，无需头文件
#endif

// 每个槽独占一个 cache line（x86/ARM 通常 64B）。
// 若不对齐，相邻槽会挤进同一 cache line：不同核写不同槽时
// 互相把对方的 line 打成 Invalid（false sharing），吞吐急剧下降。
template <typename T>
struct alignas(64) MPMCNode {
    T data{};
    // 槽位状态机。构造时初始化为槽下标 i，表示"第 0 圈的第 i 个位置为空"。
    std::atomic<size_t> sequence{0};
};

template <typename T>
class LockFreeMPMCQueue {
 public:
    explicit LockFreeMPMCQueue(size_t capacity)
        : capacity_(capacity), buffer_mask_(capacity - 1) {
        // 2 的幂判定技巧：n & (n-1) 会清掉最低位的 1，结果为 0 当且仅当
        // n 只有一个 bit（即 2^k）。mask = capacity - 1 全是低位 1。
        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("capacity must be a power of 2");
        }
        buffer_ = new MPMCNode<T>[capacity_];
        // 初始状态：slot[i].seq = i，正好满足"seq == pos → 可写"。
        // 位置从 0 开始单调递增，第 k 圈的 slot[i] 对应 pos = k*cap + i。
        // 构造函数是单线程，relaxed 足够；线程启动（join/构造 thread）
        // 本身有 happens-before，保证初始化对所有线程可见。
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~LockFreeMPMCQueue() { delete[] buffer_; }

    // 有裸 new/delete 和原子状态，拷贝语义无意义，直接禁用。
    LockFreeMPMCQueue(const LockFreeMPMCQueue&) = delete;
    LockFreeMPMCQueue& operator=(const LockFreeMPMCQueue&) = delete;

    // 入队。队满返回 false（不阻塞）。
    bool enqueue(const T& data) {
        MPMCNode<T>* node = nullptr;
        // 先拿一个候选位置。relaxed：这只是猜测值，真正的裁决在 CAS。
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        int retries = 0;

        while (true) {
            node = &buffer_[pos & buffer_mask_];
            // acquire：与消费者 dequeue 末尾的 release store 配对。
            // 一旦看到 seq == pos（槽已腾空），也保证看不到消费者
            // 还没读完的旧数据被我们覆盖的问题（消费者读 data 在
            // release 之前完成）。
            const size_t seq = node->sequence.load(std::memory_order_acquire);
            // 用带符号差值比较，而不是 seq == pos：
            // pos 单调递增会回绕（size_t 溢出），diff 在回绕时依然正确。
            const intptr_t diff = static_cast<intptr_t>(seq) -
                                  static_cast<intptr_t>(pos);

            if (diff == 0) {
                // 槽空且轮到 pos 这个票号 → 尝试占坑：把全局位置推进 1。
                // CAS 成功 = 全世界只有我拿到了这个 pos，可以独占写 slot。
                // weak：失败会重试，spurious failure 无所谓；
                // relaxed：CAS 只负责"分票"，数据可见性由 sequence 保证。
                // 注意 compare_exchange 失败时会把最新值写回 pos，
                // 所以失败后直接进下一轮循环即可。
                if (enqueue_pos_.compare_exchange_weak(
                        pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
                // CAS 失败 = 有别的生产者刚抢走这个位置，竞争激烈，
                // 指数退避降低 cache line 争抢。
                backoff(retries++);
            } else if (diff < 0) {
                // seq < pos：这个槽还停留在上一圈（消费者没腾出来）→ 队满。
                // 立即返回，把等待策略留给调用方。
                return false;
            } else {
                // seq > pos：别的生产者已经占了这个 pos 并推进了状态，
                // 我们手里的 pos 过期了 → 重新读最新位置再来。
                pos = enqueue_pos_.load(std::memory_order_relaxed);
                retries = 0;
            }
        }

        // 到这里：pos 已被我独占（CAS 赢了），但槽还未对消费者可见，
        // 可以安全地非原子写数据。
        node->data = data;
        // release 发布：seq = pos + 1 表示"数据就绪"。
        // release 保证上面的 data 写入不会被重排到这条 store 之后，
        // 消费者 acquire 读到 pos+1 时必然能看到完整数据。
        node->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // 出队。队空返回 false（不阻塞）。结构与 enqueue 完全对称。
    bool dequeue(T& data) {
        MPMCNode<T>* node = nullptr;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        int retries = 0;

        while (true) {
            node = &buffer_[pos & buffer_mask_];
            // acquire：与生产者 enqueue 末尾的 release store 配对，
            // 看到 seq == pos + 1 就保证能看到生产者写入的 data。
            const size_t seq = node->sequence.load(std::memory_order_acquire);
            // 消费者期望的状态是"数据就绪"，即 seq == pos + 1。
            const intptr_t diff = static_cast<intptr_t>(seq) -
                                  static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                // 数据就绪 → CAS 抢这个 pos 的消费权。
                if (dequeue_pos_.compare_exchange_weak(
                        pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
                backoff(retries++);
            } else if (diff < 0) {
                // seq == pos：生产者还没写到这里 → 队空。
                return false;
            } else {
                // 别的消费者已经拿走了这个 pos → 刷新位置重试。
                pos = dequeue_pos_.load(std::memory_order_relaxed);
                retries = 0;
            }
        }

        // pos 已被我独占，安全读取数据。
        data = node->data;
        // 发布"槽已腾空，等下一圈"：seq = pos + capacity。
        // 下一圈的生产者拿到 pos' = pos + capacity 时，diff == 0 → 可写。
        // release 保证上面的 data 读取完成后才把槽还给生产者
        //（防止生产者过早覆盖）。
        node->sequence.store(pos + capacity_, std::memory_order_release);
        return true;
    }

 private:
    // 指数退避：竞争失败后自旋等待 2^retries 次 pause，上限 2^10。
    // pause/yield 提示 CPU 当前在自旋：
    // - x86 _mm_pause：降低流水线冲刷开销、给同核超线程让资源；
    // - ARM yield：SMT/超线程提示，同时是编译器 barrier（"memory"）。
    // 直接空转 while 会疯狂打 cache line、拖慢持有者，反而更慢。
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
    const size_t buffer_mask_;  // capacity - 1，用于 pos & mask 取模
    MPMCNode<T>* buffer_ = nullptr;

    // 两个全局位置各占一个 cache line：
    // 生产者只狂写 enqueue_pos_、消费者只狂写 dequeue_pos_，
    // 分开对齐后两边互不干扰（否则又是 false sharing）。
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};
};

// 实际项目中槽里放的典型内容：描述符（指向 DMA buffer 的元数据），
// 而不是数据本体 —— 拷贝 16 字节 vs 拷贝整个 tensor。
struct TensorDesc {
    uint64_t dma_iova = 0;
    uint32_t byte_len = 0;
    uint16_t task_id = 0;
};

// 压力测试：4 生产者 × 4 消费者，共 1000 个元素，容量只有 16，
// 强制频繁出现队满/队空和 CAS 竞争路径。
// 验证方式：把两边收到的元素排序后逐一比对，
// 保证不丢、不重、不出现半成品数据。
bool test_mpmc_ring_buffer() {
    LockFreeMPMCQueue<uint64_t> queue(16);
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 250;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<int> produced;
    std::vector<int> consumed;
    // 测试记录用普通 vector + mutex 即可，锁不在被测路径上，
    // 不影响对无锁队列本身的并发压力。
    std::mutex produced_mutex;
    std::mutex consumed_mutex;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                // 高 32 位 = 生产者 id，低 32 位 = 序号，全局唯一，
                // 便于事后检查是否有丢失/重复。
                const uint64_t value =
                    (static_cast<uint64_t>(p) << 32) |
                    static_cast<uint64_t>(i);
                // 队满时 enqueue 返回 false → 调用方自旋 + yield 重试，
                // 这正是"无锁结构不内置等待策略"的用法示范。
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
                std::lock_guard lock(produced_mutex);
                produced.push_back(static_cast<int>(value));
            }
        });
    }

    // 消费总数计数器：4 个消费者共享退出条件（消费满 kTotal 个就停）。
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
                    // 队空 → 让出时间片，等生产者填充
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

    // 两边数量必须严格相等：不丢（produced 都被消费）、不重（没有多算）。
    assert(static_cast<int>(produced.size()) == kTotal);
    assert(static_cast<int>(consumed.size()) == kTotal);

    // MPMC 不保证全局 FIFO 顺序（多消费者取走的先后不确定），
    // 所以排序后比较集合是否一致，而不是比较顺序。
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
