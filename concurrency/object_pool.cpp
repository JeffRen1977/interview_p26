// Object pool — 预分配并复用固定数量的对象。
//
// Whiteboard talking points:
// - 构造阶段一次性创建对象，热路径 acquire()/release() 不再反复 new/delete，
//   因而可减少堆分配开销、内存碎片和延迟抖动。
// - 每个槽位包含 value 和 in_use：acquire() 把空闲槽标记为占用，
//   release() 根据对象地址找到对应槽位，再将其归还给池。
// - mutex 保护槽位状态，使多个线程可以并发借用和归还不同对象。
// - 池耗尽时支持三种行为：立即失败、等待到成功、等待到超时。
// - 本实现是面试用简化版本：等待采用 1 ms 轮询而非 condition_variable，
//   查找空闲槽和归还对象都是 O(N)。生产级实现通常使用空闲索引队列 + CV。
// - ObjectPool 只管理对象的“借用权”，返回的 T* 不拥有对象；调用方不能
//   delete，也不能在 pool 销毁后继续使用。池本身必须活得比所有借用者久。

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class ObjectPool {
 public:
    // factory 负责创建一个 T，size 是池中固定槽位数。
    // std::function 支持 lambda、函数指针和带状态的函数对象；这里的类型擦除
    // 开销只发生在初始化阶段，不进入 acquire()/release() 热路径。
    ObjectPool(std::function<T()> factory, size_t size) : items_(size) {
        // vector 在这里一次性确定大小，此后从不 resize，因此返回的
        // &item.value 地址在 ObjectPool 生命周期内保持稳定。
        for (size_t i = 0; i < size; ++i) {
            items_[i].value = factory();
            items_[i].in_use = false;
        }
    }

    // 借用一个对象：
    // - block == false：没有空闲对象时立即返回 nullptr；
    // - block == true 且 timeout 为 max：一直等待；
    // - block == true 且 timeout 有限：最多等待指定时长。
    // 成功返回池内对象的非 owning 指针，调用方完成后必须 release()。
    T* acquire(bool block = true,
               std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        // 使用 steady_clock 而不是 system_clock：系统时间可能被 NTP/用户调整，
        // steady_clock 单调递增，适合计算超时。
        // 注意：面试简化代码直接对 max() 做加法；生产代码应仅在有限超时时
        // 计算 deadline，以避免极端 duration 的溢出风险。
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            // 单独作用域让 lock_guard 在扫描结束后立即析构解锁；
            // 后面的 sleep 必须在锁外执行，否则 release() 无法拿锁归还对象，
            // 等待线程就可能永远等不到空闲槽。
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // 线性扫描寻找第一个空闲槽，复杂度 O(N)。
                for (auto& item : items_) {
                    if (!item.in_use) {
                        // “标记占用”和“返回地址”处于同一临界区，
                        // 因此两个线程不可能同时借到同一个槽。
                        item.in_use = true;
                        return &item.value;
                    }
                }
            }

            // 非阻塞模式：一次扫描失败就返回，不休眠、不重试。
            if (!block) {
                return nullptr;
            }

            if (timeout != std::chrono::milliseconds::max()) {
                // 每轮都根据绝对 deadline 重算 remaining，避免多次 sleep
                // 和调度延迟累积后超过调用者要求的总超时时间。
                const auto remaining = deadline - std::chrono::steady_clock::now();
                if (remaining <= std::chrono::steady_clock::duration::zero()) {
                    return nullptr;
                }
                // 最多睡 1 ms；若剩余时间不足 1 ms，只睡 remaining。
                // 这是 polling 方案：简单但会周期性唤醒，带来额外 CPU/功耗。
                const auto sleep_for =
                    remaining < std::chrono::milliseconds(1) ? remaining
                                                           : std::chrono::milliseconds(1);
                std::this_thread::sleep_for(sleep_for);
            } else {
                // 无限等待并不等于 busy-spin：每次失败后休眠 1 ms，
                // 但相较 condition_variable 仍有最高约 1 ms 的唤醒延迟。
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    // 归还之前由本池 acquire() 返回的对象。
    void release(T* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 通过地址做身份检查，确保不会把其他池或栈/堆上的对象放进来。
        for (auto& item : items_) {
            if (&item.value == obj) {
                // 对象本身没有析构，内部容量/资源得以保留供下一位借用者复用。
                // 当前简化实现不检查 item.in_use，因此重复 release() 不会报错；
                // 生产代码应检测 !in_use 并拒绝 double release。
                item.in_use = false;
                return;
            }
        }
        // obj 不属于 items_，拒绝修改池状态。
        throw std::invalid_argument("object does not belong to this pool");
    }

 private:
    // value 与槽位元数据放在一起。bool 受 mutex 保护，无需 atomic；
    // 若去掉 mutex，仅把 bool 改成 atomic 仍不足以完整保证借还协议正确。
    struct PooledObject {
        T value{};
        bool in_use = false;
    };

    // 实际对象存储。池销毁时 vector 析构，所有 T 才真正被析构。
    std::vector<PooledObject> items_;
    // 同时保护 items_ 中所有 in_use 状态。
    std::mutex mutex_;
};

// 验证：
// 1. 容量为 2 时只能同时借出两个 buffer；
// 2. 非阻塞 acquire(false) 在池耗尽时返回 nullptr；
// 3. release 后的槽会被下一次 acquire 复用，地址保持相同。
bool test_object_pool() {
    // factory 创建两个各含 1024 个 char 的 vector。
    // release 时 vector 不析构，所以其底层 buffer 容量也被复用。
    ObjectPool<std::vector<char>> pool(
        [] { return std::vector<char>(1024); }, 2);

    auto* b1 = pool.acquire();
    auto* b2 = pool.acquire();
    // 两个槽都已占用，非阻塞获取必须立即失败。
    assert(pool.acquire(false) == nullptr);

    pool.release(b1);
    auto* b3 = pool.acquire();
    // 线性扫描会重新找到刚释放的第一个槽。
    assert(b3 == b1);

    // 所有借用对象必须在 pool 析构前归还。
    pool.release(b2);
    pool.release(b3);
    return true;
}

int main() {
    assert(test_object_pool());
    std::cout << "object_pool: ok\n";
    return 0;
}
