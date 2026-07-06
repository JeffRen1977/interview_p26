# 24 - 无锁 SPSC 队列：内存模型与 Cacheline 对齐（数据面核心）

> **定位：** 数据面（Dataplane）无锁并发的**三件套** — SPSC 拓扑 + Acquire-Release + Cacheline 对齐。  
> **岗位：** AWS Nitro MLS / Trainium 用户态驱动、SQ/CQ、Host↔DMA 任务队列。  
> **手撕代码：** [interview_handwrite/cpp/spsc_ring_buffer.cpp](../interview_handwrite/cpp/spsc_ring_buffer.cpp)  
> **关联：** [21-用户态驱动](./21-Trainium-用户态数据面驱动架构.md) | [amazon_cpp/06-并发](../amazon_cpp/docs/06-并发与内存模型.md)

---

## 0. 面试通关一句话

> **SPSC** 从拓扑上消除写-写竞争；**Acquire-Release** 在乱序 CPU 上保证数据可见顺序；**Cacheline 对齐** 从硬件上消灭 False Sharing。三者合一，才是生产级数据面无锁队列。

---

## 0.1 答题框架（20–30 min 手撕 + 讲解）

| 阶段 | 时间 | 内容 |
|------|------|------|
| 画 Ring Buffer | 3 min | head/tail、N+1 slot |
| 写 push/pop | 10 min | relaxed + acquire/release |
| 讲 memory order | 5 min | 为何 tail 用 release |
| 讲 false sharing | 5 min | alignas(64) 隔离 head/tail |
| Follow-up | 5 min | SPSC vs MPMC、ABA、与 21 文档 SQ 关系 |

---

# 第一部分：SPSC 场景基石

## 1.1 为什么是 SPSC，不是 MPMC？

在 AI 训练驱动和网络数据面中，最常见的是**解耦流水线**，而非多线程抢一把锁：

```
Core A (Producer)          Core B (Consumer)
  PyTorch / 驱动填 SQ  →  Ring Buffer  →  DMA / 硬件拉取
       只写 tail               缓冲区          只写 head
```

| 模型 | 写者 | 读者 | 同步 |
|------|------|------|------|
| **SPSC** | 1 个线程写 tail | 1 个线程写 head | 无锁 atomic |
| **MPSC** | 多写 tail | 1 读 head | CAS 或锁 |
| **MPMC** | 多写 | 多读 | 锁或复杂 lock-free |

**Nitro/Trainium 实例：**
- 每线程 **Queue Pair**（[21-文档](./21-Trainium-用户态数据面驱动架构.md)）→ 天然 SPSC
- Host 填 Submission Queue，DMA 引擎消费 → 逻辑 SPSC
- 不要用 MPMC 解决本可用 SPSC 拆分的问题

## 1.2 环形缓冲区结构

```
capacity = N  →  分配 N+1 个 slot（留一个空位区分满/空）

      head                           tail
        ↓                             ↓
      [X][X][X][ ][ ]   (size = 5, capacity = 4)
```

| 条件 | 判断 |
|------|------|
| **空** | `head == tail` |
| **满** | `(tail + 1) % size == head` |

## 1.3 为何 SPSC 可以无锁？

| 指针 | 谁写 | 谁读 |
|------|------|------|
| `tail` | **仅 Producer** | Consumer 只读 |
| `head` | **仅 Consumer** | Producer 只读 |

**没有写-写冲突** → 不需要 `mutex`，不需要对 head/tail 做 CAS 竞争。

**仍需注意：** 读-写之间要通过 **memory order** 保证「数据先于指针发布」；多核上要通过 **cacheline 对齐** 避免 false sharing。

---

# 第二部分：C++ 内存模型

## 2.1 为什么光有 SPSC 还不够？

现代 CPU 会：
1. **指令重排**（编译器 + CPU Out-of-Order）
2. **缓存延迟写入**（Store Buffer）

### 致命场景（无 memory order）

```cpp
// Producer
buffer[tail] = tensor_desc;   // (1) 写数据
tail++;                       // (2) 可能被重排到 (1) 之前！
```

Consumer 看到 `tail` 已前进，但 `buffer[tail]` 仍是旧数据 → **读到垃圾指针 → Crash**。

## 2.2 Acquire-Release 语义

### 生产者（发布 Publish）

```cpp
buffer_[tail] = item;  // 写 slot 数据
tail_.store(next_tail, std::memory_order_release);
// release: 此 store 之前的所有写，不能重排到此 store 之后
```

### 消费者（获取 Acquire）

```cpp
const size_t t = tail_.load(std::memory_order_acquire);
// acquire: 此 load 之后的读写，能看到 release 之前的所有写
if (head == t) return empty;
item = buffer_[head];
head_.store(next_head, std::memory_order_release);
```

### 单向屏障直觉

```
Producer                         Consumer
────────                         ────────
write data ──┐
             ├── release(tail) ──→ acquire(read tail) ──→ see data
```

**不需要**默认的 `memory_order_seq_cst`（全局顺序，更慢）；SPSC 只需这对 release-acquire 配对。

## 2.3 完整 push/pop 的 order 选择

```cpp
bool push(const T& item) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t next = (tail + 1) % size_;
    if (next == head_.load(std::memory_order_acquire))  // 读对方指针：acquire
        return false;
    buffer_[tail] = item;
    tail_.store(next, std::memory_order_release);       // 发布：release
    return true;
}

std::optional<T> pop() {
    const size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire))  // 读对方指针：acquire
        return std::nullopt;
    T item = std::move(buffer_[head]);
    head_.store((head + 1) % size_, std::memory_order_release);
    return item;
}
```

| 操作 | order | 原因 |
|------|-------|------|
| 读**自己的**指针 | `relaxed` | 仅自己写，无同步需求 |
| 读**对方的**指针 | `acquire` | 建立 happens-before |
| 写**自己的**指针 | `release` | 发布此前 slot 写入 |

## 2.4 与其他 memory order 对比

| order | 成本 | SPSC 是否需要 |
|-------|------|---------------|
| `relaxed` | 最低 | 仅读自己指针 |
| `release` / `acquire` | 中 | **配对使用** |
| `acq_rel` | 中 | RMW 时 |
| `seq_cst` | 最高 | 默认；SPSC 可不用 |

## 2.5 高频 Follow-up

**Q: `volatile` 能代替 atomic 吗？**  
不行。`volatile` 不保证原子性、不建立线程间 happens-before。

**Q: 为什么不用 mutex？**  
mutex 涉及 syscall/futex、上下文切换；数据面热路径要 μs 级延迟。

**Q: SPSC 需要 CAS 吗？**  
不需要。CAS 用于多写者竞争同一变量（MPMC/MPSC）。

---

# 第三部分：Cacheline 对齐与 False Sharing

## 3.1 硬件事实

- CPU 以 **Cacheline（通常 64B）** 为单位加载内存
- 多核一致性：**MESI** 协议 — 一核写某 line → 其他核该 line **失效**

## 3.2 False Sharing 惨剧

```cpp
// ❌ 糟糕：head 和 tail 很可能在同一 cacheline
struct Bad {
    std::atomic<size_t> head;  // 8B
    std::atomic<size_t> tail;  // 8B
};
```

```
1. Core1 (Producer) 写 tail → 整条 64B line 在其他核失效
2. Core2 (Consumer) 写 head → 必须重新从 L3/内存拉 line
3. 两核乒乓颠簸 (Cache Bouncing) — 比加锁还慢
```

## 3.3 解法：强制隔离

```cpp
#include <new>

class SPSCRingBufferAligned {
    // C++17：编译器报告避免 false sharing 的最小偏移
    alignas(std::hardware_destructive_interference_size)
    std::atomic<size_t> head_{0};

    alignas(std::hardware_destructive_interference_size)
    std::atomic<size_t> tail_{0};

    // buffer 另占内存区域，与 head/tail 无关
    std::vector<T> buffer_;
};
```

| 写法 | 说明 |
|------|------|
| `alignas(64)` | x86_64 / ARM64 通用做法 |
| `hardware_destructive_interference_size` | C++17 标准常量（通常 64） |
| `hardware_constructive_interference_size` | 希望放同一 line 时用（少见） |

**Graviton (ARM)：** 同样 64B cacheline；用 `yield` 自旋（见 [21-文档 polling](./21-Trainium-用户态数据面驱动架构.md)）。

## 3.4 True Sharing vs False Sharing

| | True Sharing | False Sharing |
|---|--------------|---------------|
| 含义 | 线程逻辑上共享同一变量 | 线程写**不同**变量但同一 cacheline |
| 解决 | 必须同步（atomic/mutex） | **对齐隔离** |

---

# 第四部分：生产级完整实现（面试白板版）

```cpp
#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <vector>

template <typename T>
class SPSCRingBuffer {
 public:
    explicit SPSCRingBuffer(size_t capacity)
        : size_(capacity + 1), buffer_(size_) {
        if (capacity == 0) throw std::invalid_argument("capacity");
    }

    bool push(const T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next = (tail + 1) % size_;
        if (next == head_.load(std::memory_order_acquire))
            return false;
        buffer_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))
            return std::nullopt;
        T item = std::move(buffer_[head]);
        head_.store((head + 1) % size_, std::memory_order_release);
        return item;
    }

 private:
    static constexpr size_t kCacheLine =
        std::hardware_destructive_interference_size;  // fallback: 64

    size_t size_;
    std::vector<T> buffer_;

    alignas(kCacheLine) std::atomic<size_t> head_{0};
    alignas(kCacheLine) std::atomic<size_t> tail_{0};
};
```

**与仓库现有代码：** [spsc_ring_buffer.cpp](../interview_handwrite/cpp/spsc_ring_buffer.cpp) 逻辑正确；面试加分可主动提出 **head/tail 应 alignas 隔离**。

---

# 第五部分：与 Nitro 数据面架构的映射

```
┌─────────────────────────────────────────────────────────┐
│ 21-文档：TrainiumDesc 写入 SQ slot（Producer 线程）      │
│   tail_.store(..., release)                             │
├─────────────────────────────────────────────────────────┤
│ Ring Buffer = Submission Queue 环形描述符数组            │
├─────────────────────────────────────────────────────────┤
│ Consumer：DMA 引擎或 polling 线程读 head、拉描述符        │
│   tail_.load(..., acquire)                              │
└─────────────────────────────────────────────────────────┘
```

| 概念 | SPSC 队列 | 21-用户态驱动 |
|------|-----------|---------------|
| slot 数据 | `T` / 描述符 | `TrainiumDesc` |
| 满/空 | head/tail | SQ 满则 backpressure |
| 发布 | release + doorbell | doorbell 在 release 之后 |
| 对齐 | head/tail 分 line | desc 数组 32B/64B 对齐（PCIe TLP） |

---

# 第六部分：面试题速查

| # | 问题 | 答案要点 |
|---|------|----------|
| 1 | SPSC 为何无锁？ | 单写 tail、单写 head，无写-写冲突 |
| 2 | 为何 N+1 slot？ | 区分满/空 |
| 3 | release/acquire 配对？ | 数据写完 release tail；对方 acquire 读 tail 后见数据 |
| 4 | 为何读自己指针用 relaxed？ | 只有自己写 |
| 5 | False sharing？ | 不同变量同 cacheline；alignas(64) |
| 6 | SPSC vs MPMC？ | 多生产者需 CAS 或锁；优先拆成多个 SPSC |
| 7 | 比 mutex 快多少？ | 无 futex；但 false sharing 可更慢 — 必须对齐 |
| 8 | ABA 问题？ | 纯 SPSC 指针递增通常无 ABA；MPMC 需 generation |
| 9 | 如何 backpressure？ | push 返回 false；上层 spin/yield/等 CQ |
| 10 | seq_cst 何时用？ | 不确定时用；SPSC 不需要 |

---

# 第七部分：白板手写顺序

1. 画 ring、`head`/`tail`、N+1（2 min）  
2. 写 `push`：acquire 读 head、写 slot、release 写 tail（5 min）  
3. 写 `pop`：对称（3 min）  
4. 讲乱序失败案例（2 min）  
5. 加 `alignas(64)` 到 head/tail（3 min）  
6. 联系到 Trainium SQ / doorbell（2 min）

---

# 第八部分：一页纸速记

```
SPSC:  单写 tail，单写 head → 无写-写冲突 → 不需 mutex/CAS

Ring:  size = N+1；满 (tail+1)%size==head；空 head==tail

Order: 写 slot → tail.release()；对方 tail.acquire() 后读 slot
       读自己指针 relaxed；读对方指针 acquire

False sharing: head/tail 同 cacheline → 核间颠簸
Fix: alignas(64) 或 hardware_destructive_interference_size

数据面: 每核一对 SPSC QP；DMA 消费；release 后再 doorbell
```

---

## 相关资源

| 主题 | 路径 |
|------|------|
| 可运行 SPSC | [spsc_ring_buffer.cpp](../interview_handwrite/cpp/spsc_ring_buffer.cpp) |
| MPMC mutex 版对比 | [thread_safe_ring_buffer.cpp](../interview_handwrite/cpp/thread_safe_ring_buffer.cpp) |
| 并发 memory model | [amazon_cpp/docs/06](../amazon_cpp/docs/06-并发与内存模型.md) |
| SQ/CQ 驱动 | [21](./21-Trainium-用户态数据面驱动架构.md) |
| 题库 A2 | [20-题库](./20-Trainium-Nitro-MLS-硬核面试题库.md) |
