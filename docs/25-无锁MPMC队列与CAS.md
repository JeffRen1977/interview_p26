# 25 - 无锁 MPMC 队列：序列号 + CAS（海量数据面）

> **场景：** 多 Data Loader 向驱动提交 Tensor 描述符；多 DMA 通道并发消费 — **MPMC**（Multi-Producer Multi-Consumer）。  
> **与 SPSC 关系：** SPSC 是 per-thread QP 的最优解；MPMC 用于必须多对多共享一条队列时。  
> **手撕代码：** [interview_handwrite/cpp/mpmc_ring_buffer.cpp](../interview_handwrite/cpp/mpmc_ring_buffer.cpp)  
> **关联：** [24-SPSC](./24-无锁SPSC队列与Cacheline对齐.md) | [21-用户态驱动](./21-Trainium-用户态数据面驱动架构.md) | [20-题库 G3](./20-Trainium-Nitro-MLS-硬核面试题库.md)

---

## 0. 面试通关一句话

> MPMC 不能靠单写 head/tail，必须用 **每槽位 sequence** + **CAS 抢全局 enqueue/dequeue 位置**；队列里只传 **DMA 描述符/指针**，不传 Tensor 本体；高竞争时 CAS 失败要 **指数退避 + yield**。

---

## 0.1 SPSC vs MPMC 选型

| 场景 | 推荐 | 原因 |
|------|------|------|
| 每核独立 QP、Host↔单 DMA 通道 | **SPSC** ([24](./24-无锁SPSC队列与Cacheline对齐.md)) | 无 CAS、最低延迟 |
| 多 Loader 共享一条提交队列 | **MPMC**（本文） | 必须多写者竞争 |
| 正确性优先、吞吐要求一般 | **Mutex ring** ([thread_safe_ring_buffer.cpp](../interview_handwrite/cpp/thread_safe_ring_buffer.cpp)) | 简单可证明 |
| 万卡数据面最优实践 | **拆成 N 条 SPSC** | 能拆就不上 MPMC |

**面试金句：** 先问能否 per-core SPSC；不能才上 MPMC sequence queue。

---

## 0.2 答题框架（30–40 min）

| 阶段 | 时间 | 内容 |
|------|------|------|
| 为何 SPSC 不够 | 5 min | 多写 tail 竞争 |
| 画 Slot + sequence | 5 min | 防抢跑、防踩踏 |
| 写 enqueue/dequeue | 15 min | CAS + diff 三分支 |
| Nitro 优化 | 10 min | 描述符、退避、weak CAS |
| vs Disruptor / mutex | 5 min | trade-off |

---

# 第一部分：MPMC 为何需要序列号？

## 1.1 普通 head/tail 的致命漏洞

```
线程 A CAS 抢到 slot 1 写入权
  → 尚未写入 Tensor 描述符
  → 被 OS 抢占
消费者看到 head 指向 slot 1
  → 读出脏数据 → Crash
```

**根因：** 全局 `tail++` 只表示「槽位归属」，不表示「数据已就绪」。

## 1.2 每槽位独立 sequence

```
Slot Array (capacity = 4, 下标 0..3):

+-------------+-------------+-------------+-------------+
| Slot 0      | Slot 1      | Slot 2      | Slot 3      |
| data        | data        | data        | data        |
| seq: 0→1→4  | seq: 1→2→5  | seq: 2→3→6  | seq: 3→4→7  |
+-------------+-------------+-------------+-------------+
     ^                                           ^
 dequeue_pos=0                              enqueue_pos=2
```

**不变量（一轮循环）：**

| 状态 | sequence 与 pos 关系 | 含义 |
|------|----------------------|------|
| 可写（空槽） | `seq == pos` | 消费者已释放，生产者可填 |
| 可读（满数据） | `seq == pos + 1` | 生产者已发布 |
| 队列满 | `seq < pos`（enqueue 侧） | 消费者落后一整圈 |
| 队列空 | `seq < pos + 1`（dequeue 侧） | 生产者未就绪 |

**初始化：** `buffer_[i].sequence = i`

**enqueue 完成后：** `sequence = pos + 1`（release）

**dequeue 完成后：** `sequence = pos + capacity`（下一轮该槽位的可写序号）

---

# 第二部分：数据结构设计

```cpp
template <typename T>
struct alignas(64) Node {
    T data;
    std::atomic<size_t> sequence;
};

template <typename T>
class LockFreeMPMCQueue {
    const size_t capacity_;      // 2 的幂
    const size_t buffer_mask_;   // capacity - 1，用 & 代替 %
    Node<T>* buffer_;

    alignas(64) std::atomic<size_t> enqueue_pos_;
    alignas(64) std::atomic<size_t> dequeue_pos_;
};
```

| 设计点 | 原因 |
|--------|------|
| `capacity` 为 2 的幂 | `pos & mask` 比 `%` 快 |
| `Node` alignas(64) | 减少相邻 slot 伪共享（大 T 时 node 本身已跨 line） |
| `enqueue_pos_` / `dequeue_pos_` 分 line | 生产者与消费者控制变量隔离 |
| `T` = 描述符而非 Tensor | 见 §5.2 |

---

# 第三部分：Enqueue / Dequeue 控制流

## 3.1 多生产者 Enqueue

```cpp
bool enqueue(const T& data) {
    Node<T>* node;
    size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

    while (true) {
        node = &buffer_[pos & buffer_mask_];
        size_t seq = node->sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) -
                        static_cast<intptr_t>(pos);

        if (diff == 0) {
            // 槽位空，尝试抢占写入位置 pos
            if (enqueue_pos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                break;
            }
            // CAS 失败：别的生产者推进了 enqueue_pos，循环重试
        } else if (diff < 0) {
            return false;  // 队列满
        } else {
            pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
    }

    node->data = data;  // 独占槽位，无竞争
    node->sequence.store(pos + 1, std::memory_order_release);
    return true;
}
```

**`diff` 三分支直觉：**

| diff | 含义 | 动作 |
|------|------|------|
| `== 0` | 槽位可写 | CAS 抢 `enqueue_pos` |
| `< 0` | 满 | 返回 false |
| `> 0` | 本地 pos 过期 | 重载 `enqueue_pos` |

## 3.2 多消费者 Dequeue

```cpp
bool dequeue(T& data) {
    Node<T>* node;
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

    while (true) {
        node = &buffer_[pos & buffer_mask_];
        size_t seq = node->sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) -
                        static_cast<intptr_t>(pos + 1);

        if (diff == 0) {
            if (dequeue_pos_.compare_exchange_weak(
                    pos, pos + 1, std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return false;  // 队列空
        } else {
            pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
    }

    data = node->data;
    node->sequence.store(pos + capacity_, std::memory_order_release);
    return true;
}
```

## 3.3 时序：单 Slot 生命周期

```
轮次 k，slot i 的下标 pos = k*capacity + i

初始:     seq = pos
enqueue:  写 data → seq = pos + 1     (消费者可见)
dequeue:  读 data → seq = pos + cap  (生产者下轮可写)
```

---

# 第四部分：与 SPSC 的对比

| | SPSC ([24](./24-无锁SPSC队列与Cacheline对齐.md)) | MPMC（本文） |
|---|-----------------------------------------------|--------------|
| head/tail 写者 | 各 1 个 | 多线程 CAS 抢 pos |
| 槽位就绪 | release/acquire 即可 | **sequence 状态机** |
| 复杂度 | 低 | 高 |
| 热路径指令 | 无 CAS | CAS 循环 |
| 适用 | per-core QP | 共享提交队列 |

---

# 第五部分：Nitro 级优化（面试加分）

## 5.1 高竞争 CAS：指数退避

**问题：** 上百线程 `compare_exchange_weak` 失败 → 总线风暴、cache coherence traffic、p99 延迟飙升。

```cpp
#if defined(__x86_64__)
    #include <immintrin.h>
    static void cpu_pause() { _mm_pause(); }
#elif defined(__aarch64__)
    static void cpu_pause() {
        __asm__ __volatile__("yield" ::: "memory");
    }
#endif

// CAS 失败分支：
// for (int i = 0; i < (1 << retry); ++i) cpu_pause();
// retry = min(retry + 1, MAX_BACKOFF);
```

**面试表达：**
> 拥堵时指数退避 + `pause`/`yield`，把无效 CAS 从总线上卸下来，极高竞争下吞吐可提升 30%+（需实测）。

## 5.2 伪零拷贝：队列只传描述符

```cpp
struct TensorDesc {
    uint64_t dma_iova;   // IOMMU 地址，非虚拟指针
    uint32_t byte_len;
    uint16_t task_id;
    uint16_t flags;
};

LockFreeMPMCQueue<TensorDesc> submit_queue;
```

**原则：**
- `T` **绝不是** 大 Tensor 值类型
- Data Loader 只 push 描述符；DMA 引擎 dequeue 后直接按 IOVA 拉数据
- 与 [21-文档](./21-Trainium-用户态数据面驱动架构.md) HugePage pool 一体

## 5.3 `compare_exchange_weak` vs `strong`

| | weak | strong |
|---|------|--------|
| 伪失败 | 允许 | 不允许 |
| 硬件成本 | 更低 | 更重 |
| 适用 | **`while(true)` 循环内** | 单次尝试 |

**面试表达：**
> 循环里用 `weak`；失败就重载 pos 再试。`strong` 留给不能容忍伪失败的一次性逻辑。

## 5.4 其他生产考量

| 话题 | 要点 |
|------|------|
| **ABA** | sequence 递增，非单纯指针 CAS；仍注意 32-bit pos 溢出（用 64-bit） |
| **公平性** | MPMC 无严格 FIFO 公平；高竞争下可能饥饿 |
| **批量 dequeue** | 一次抢连续 slot 减 CAS 次数 |
| **NUMA** | 队列内存分配在消费者/生产者同 node |
| **回退方案** | 竞争过高时拆队列或 mutex 降级 |

---

# 第六部分：架构选型（21 / 24 / 25 串联）

```
                    多 Data Loader
                          │
            ┌─────────────┴─────────────┐
            ▼                           ▼
    [MPMC 共享队列]              [Per-core SPSC × N]  ← 优先
    本文 §5                      24-文档
            │                           │
            └─────────────┬─────────────┘
                          ▼
              TrainiumDesc → SQ (21-文档)
                          ▼
                    DMA / Trainium
```

**G3 系统设计题：** [20-题库](./20-Trainium-Nitro-MLS-硬核面试题库.md) → 先 mutex 版正确，再讲 sequence MPMC 升级。

---

# 第七部分：面试题速查

| # | 问题 | 要点 |
|---|------|------|
| 1 | MPMC 为何不能只用 head/tail？ | 多写者；槽位就绪与 pos 不同步 |
| 2 | sequence 作用？ | 状态机：空/满/满队列/空队列 |
| 3 | diff == 0 含义？ | enqueue: 可写；dequeue: 可读 |
| 4 | 为何 capacity 是 2 幂？ | 位掩码取模 |
| 5 | CAS 失败怎么办？ | 重载 pos；退避 |
| 6 | weak vs strong？ | 循环用 weak |
| 7 | 队列存什么？ | 描述符/IOVA，不存 Tensor |
| 8 | vs mutex ring？ | MPMC lock-free 高竞争才值得 |
| 9 | vs 拆 SPSC？ | 能拆则拆 |
| 10 | 与 Disruptor？ | 同属 sequence ring；Disruptor 偏 JVM 生态 |

---

# 第八部分：白板手写顺序

1. 解释多 Loader 为何 SPSC 不够（3 min）  
2. 画 slot + sequence 状态（5 min）  
3. 写 enqueue 三分支 + CAS（10 min）  
4. 写 dequeue 对称逻辑（8 min）  
5. 描述符 + 退避 + weak CAS（5 min）  
6. 说「生产优先拆 SPSC」（2 min）

---

# 第九部分：一页纸速记

```
MPMC: 多写 enqueue_pos、多写 dequeue_pos → 必须 CAS + per-slot sequence

初始化: buffer[i].seq = i
Enqueue: 等 seq==pos → CAS enqueue_pos → 写 data → seq=pos+1 (release)
Dequeue: 等 seq==pos+1 → CAS dequeue_pos → 读 data → seq=pos+capacity (release)
满: seq < pos (enqueue)    空: seq < pos+1 (dequeue)

优化: T=TensorDesc/IOVA; alignas(64); CAS失败退避; compare_exchange_weak
首选: 能 per-core SPSC 就不上 MPMC
```

---

## 相关资源

| 主题 | 路径 |
|------|------|
| MPMC 实现 | [mpmc_ring_buffer.cpp](../interview_handwrite/cpp/mpmc_ring_buffer.cpp) |
| SPSC 对照 | [24](./24-无锁SPSC队列与Cacheline对齐.md) |
| Mutex MPMC | [thread_safe_ring_buffer.cpp](../interview_handwrite/cpp/thread_safe_ring_buffer.cpp) |
| SQ 驱动 | [21](./21-Trainium-用户态数据面驱动架构.md) |

**参考算法：** Dmitry Vyukov bounded MPMC queue（sequence lock-free ring）。
