# Amazon C++ 面试题型全景（Systems / Embedded）

> **岗位：** AWS / Annapurna / Lab126 / Devices / Robotics 等偏底层 C++  
> **风格：** 低层实现细节 · 内存效率 · 少依赖 STL · 讲清内存布局与性能  
> **语言深度：** [README.md](../README.md) 18 Part 文档 · **Coding 高频：** [12-EC2-Nitro-Coding高频题.md](./12-EC2-Nitro-Coding高频题.md)  
> **工程手撕：** [../../interview_handwrite/](../../interview_handwrite/)

---

## 0. 嵌入式 / 系统岗面试官在看什么

| Focus | 典型追问 |
|-------|----------|
| Memory management | 谁拥有、谁释放、泄漏路径、碎片 |
| Pointer manipulation | 算术、对齐、dangling、别名 |
| Bit operations | endian、mask、power-of-2 |
| DS from scratch | 不用 `std::list` / `std::unordered_map` 时怎么写 |
| Thread safety | mutex vs lock-free、memory_order、ABA |
| Performance | cache line、false sharing、零拷贝 |

**白板习惯（每次都做）：** 画图 → 权衡 → 错误处理 → 内存归属 → 线程安全 → 可扩展 → 性能含义。

---

## 1. Coding 题型清单（算法 + 语言）

### 1.1 Data Structures — Linked Lists

| 题型 | 状态 | 本仓 / 建议 |
|------|------|-------------|
| Singly / Doubly list 手写 | ☐ | [lru_cache_raw_list.cpp](../../interview_handwrite/lru_cache_raw_list.cpp) |
| Reverse linked list | ☐ | LeetCode 206 → [`leetcode/`](../../leetcode/) |
| Detect cycle | ☐ | LC 141/142 |
| Merge two sorted lists | ☐ | LC 21 |
| Delete nth from end | ☐ | LC 19 |

**系统岗加分：** 哨兵节点、O(1) 删除、与 hashmap 组合（LRU）。

### 1.2 Trees

| 题型 | 状态 | 建议 |
|------|------|------|
| BT / BST 基础 | ☐ | 白板画左右孩子 + parent（可选） |
| Inorder / Preorder / Postorder | ☐ | 递归 + 迭代（栈）都能写 |
| Is balanced | ☐ | LC 110，返回高度同时判平衡 |
| Lowest common ancestor | ☐ | LC 236 / 235 |
| Level order | ☐ | LC 102，队列 |
| Is BST | ☐ | LC 98，带上下界 |

### 1.3 Arrays and Strings

| 题型 | 状态 | 本仓 / 建议 |
|------|------|-------------|
| 不用库函数做字符串操作 | ☐ | [15_valid_ip.cpp](../examples/15_valid_ip.cpp) |
| Array rotation | ☐ | LC 189 |
| Finding duplicates | ☐ | 位图 / 原地交换；[17_bitmap.cpp](../examples/17_bitmap.cpp) |
| Substring | ☐ | 双指针 / sliding window |
| Buffer overflow 场景 | ☐ | 讲清边界检查、`strncpy` 陷阱、[02-内存指针与布局.md](./02-内存指针与布局.md) |

### 1.4 Bit Manipulation

| 题型 | 状态 | 本仓 / 建议 |
|------|------|-------------|
| Count set bits | ☐ | Brian Kernighan / `__builtin_popcount` |
| Power of 2 | ☐ | `n > 0 && (n & (n-1)) == 0` |
| Bit shifting | ☐ | 算术 vs 逻辑右移；符号扩展 |
| Endianness | ☐ | 联合体 / 字节交换；[12-EC2-Nitro](./12-EC2-Nitro-Coding高频题.md) |

### 1.5 Stack and Queue

| 题型 | 状态 | 本仓 / 建议 |
|------|------|-------------|
| Stack using queues | ☐ | LC 225 |
| Queue using stacks | ☐ | LC 232 |
| Min/Max stack | ☐ | LC 155 |
| 有界阻塞队列 | ☐ | [bounded_blocking_queue.cpp](../../interview_handwrite/bounded_blocking_queue.cpp) |
| 线程安全 ring | ☐ | [thread_safe_ring_buffer.cpp](../../interview_handwrite/thread_safe_ring_buffer.cpp) · SPSC/MPMC |

### 1.6 Graph

| 题型 | 状态 | 建议 |
|------|------|------|
| DFS / BFS | ☐ | 邻接表；嵌入式常考「依赖图」 |
| Cycle detection | ☐ | 有向：三色 / 拓扑；无向：parent |
| Shortest path | ☐ | BFS（无权）/ Dijkstra |
| Topological sort | ☐ | Kahn / DFS；构建系统、任务调度 |

### 1.7 Heap（常考加项）

| 题型 | 状态 | 本仓 |
|------|------|------|
| Min heap from scratch | ☐ | [16_min_heap.cpp](../examples/16_min_heap.cpp) |

---

## 2. C++ 语言与指针（必考口述 + 小代码）

### 2.1 Memory Management

| 考点 | 状态 | 本仓 |
|------|------|------|
| Memory leaks 如何发现 / 避免 | ☐ | RAII；[01-对象模型与生命周期.md](./01-对象模型与生命周期.md) |
| Smart pointers 用法与陷阱 | ☐ | [06_smart_pointers.cpp](../examples/06_smart_pointers.cpp) · [shared_ptr.cpp](../../interview_handwrite/shared_ptr.cpp) |
| Stack vs Heap | ☐ | [02-内存指针与布局.md](./02-内存指针与布局.md) · [02_memory_placement.cpp](../examples/02_memory_placement.cpp) |
| Deep vs Shallow copy | ☐ | [01_rule_of_five.cpp](../examples/01_rule_of_five.cpp) |

### 2.2 Pointers

| 考点 | 状态 | 本仓 |
|------|------|------|
| Pointer arithmetic | ☐ | [02](./02-内存指针与布局.md) · [03_const_pointers.cpp](../examples/03_const_pointers.cpp) |
| Dangling pointers | ☐ | 返回局部地址、use-after-free |
| Function pointers | ☐ | 回调 / 状态机表驱动 |
| Memory alignment | ☐ | [08_memory_layout.cpp](../examples/08_memory_layout.cpp) · [14_aligned_malloc.cpp](../examples/14_aligned_malloc.cpp) |

### 2.3 Common C++ Concepts

| 考点 | 状态 | 本仓 |
|------|------|------|
| Virtual functions / vtable | ☐ | [03-虚函数与类型转换.md](./03-虚函数与类型转换.md) · [07_virtual_vtable.cpp](../examples/07_virtual_vtable.cpp) |
| Constructor / Destructor chains | ☐ | 虚析构、成员析构顺序 |
| Operator overloading | ☐ | [04_overload_resolution.cpp](../examples/04_overload_resolution.cpp) |
| Templates | ☐ | [05-模板与异常.md](./05-模板与异常.md) |

---

## 3. C++ System Design（Systems / Embedded）

### 3.1 Low-Level Components（优先手撕）

| 组件 | 状态 | 本仓 |
|------|------|------|
| Memory allocator / pool | ☐ | [09-无锁固定大小内存池.md](./09-无锁固定大小内存池.md) · [11_lock_free_fixed_pool.cpp](../examples/11_lock_free_fixed_pool.cpp) · [two_level_mempool.cpp](../../interview_handwrite/two_level_mempool.cpp) |
| Object pool | ☐ | [object_pool.cpp](../../interview_handwrite/object_pool.cpp) |
| Thread-safe queue | ☐ | [bounded_blocking_queue.cpp](../../interview_handwrite/bounded_blocking_queue.cpp) |
| Lock-free ring (SPSC/MPMC) | ☐ | [spsc_ring_buffer.cpp](../../interview_handwrite/spsc_ring_buffer.cpp) · [mpmc_ring_buffer.cpp](../../interview_handwrite/mpmc_ring_buffer.cpp) |
| Smart pointer | ☐ | [shared_ptr.cpp](../../interview_handwrite/shared_ptr.cpp) |
| Thread-safe LRU | ☐ | [lru_cache_ds.cpp](../../interview_handwrite/lru_cache_ds.cpp) · [lru_cache_raw_list.cpp](../../interview_handwrite/lru_cache_raw_list.cpp) |
| Rate limiter | ☐ | [11-令牌桶限流器.md](./11-令牌桶限流器.md) · [13_token_bucket_rate_limiter.cpp](../examples/13_token_bucket_rate_limiter.cpp) |
| Buffer / memmove | ☐ | [10-memcpy与memmove.md](./10-memcpy与memmove.md) · [12_memmove.cpp](../examples/12_memmove.cpp) |
| Thread pool | ☐ | [07-Linux系统与设计题.md](./07-Linux系统与设计题.md) |
| Event dispatcher / Reactor | ☐ | [18_event_dispatcher.cpp](../examples/18_event_dispatcher.cpp)；epoll 见 doc 07 |
| State machine | ☐ | 表驱动 + 函数指针；嵌入式常考 |

### 3.2 Concurrency

| 考点 | 状态 | 本仓 |
|------|------|------|
| Mutex vs Semaphore | ☐ | [06-并发与内存模型.md](./06-并发与内存模型.md) |
| Reader-writer locks | ☐ | `shared_mutex`；写饥饿 |
| Condition variables | ☐ | [10_concurrency_atomic.cpp](../examples/10_concurrency_atomic.cpp) · 阻塞队列 |
| Deadlock prevention | ☐ | 锁顺序、try_lock、超时 |
| Lock-free + ABA | ☐ | [two_level_mempool.cpp](../../interview_handwrite/two_level_mempool.cpp) ABA 注释 · doc 09 |
| Memory barriers / false sharing | ☐ | [06](./06-并发与内存模型.md) · examples/10 |

### 3.3 Design Patterns（能白板实现）

| Pattern | 状态 | 系统岗要点 |
|---------|------|------------|
| Singleton（thread-safe） | ☐ | Meyer's / `call_once`；慎用全局 |
| Factory | ☐ | 与虚接口 + 异构设备 |
| Observer | ☐ | 回调生命周期；弱引用 |
| **RAII** | ☐ | 必考；锁、fd、mmap |
| Producer-Consumer | ☐ | 有界队列 + CV |
| Command | ☐ | 任务队列、可撤销 |

### 3.4 Performance Optimization

| 考点 | 状态 | 本仓 |
|------|------|------|
| Memory layout / SoA vs AoS | ☐ | [08_memory_layout.cpp](../examples/08_memory_layout.cpp) |
| Cache-friendly structures | ☐ | [06](./06-并发与内存模型.md) Cache 节 |
| Alignment | ☐ | [14_aligned_malloc.cpp](../examples/14_aligned_malloc.cpp) |
| Zero-copy | ☐ | dma-buf 口述；ring 只传描述符 |
| Lock contention / batching | ☐ | Per-core cache；bulk refill（两级 mempool） |

### 3.5 重要概念速记

| 概念 | 一句话 | 文档 |
|------|--------|------|
| SOLID | 接口隔离在设备驱动/HAL 很常见 | — |
| Memory model | acquire/release vs seq_cst | [06](./06-并发与内存模型.md) |
| Cache coherency | MESI；跨核写为何贵 | [06](./06-并发与内存模型.md) |
| Memory barriers | 防重排；与 atomic 绑定 | [06](./06-并发与内存模型.md) |
| ABA | 指针复用骗过 CAS → tagged pointer | [09](./09-无锁固定大小内存池.md) |
| False sharing | 两原子挤同一 cache line | examples/10 |

### 3.6 Real-world 设计题（口述框架）

| 题目 | 入口 |
|------|------|
| Rate limiter | [11](./11-令牌桶限流器.md) |
| Smart pointers | handwrite `shared_ptr` |
| Thread-safe LRU | handwrite LRU |
| Memory management system | pool + aligned_malloc + 碎片策略 |
| File system（概要） | inode / 块缓存 / 日志 — 偏原理 |
| Device driver architecture | 用户态/内核、中断下半部、零拷贝 |

---

## 4. 两周冲刺日程（可勾选）

### Week A — Coding + 语言

| Day | 主题 | 动作 |
|-----|------|------|
| 1 | List + 指针陷阱 | Reverse / Cycle / Merge；口述 dangling |
| 2 | Tree | 遍历 + LCA + isBST；迭代版至少一题 |
| 3 | Array/String/Bit | valid_ip、bitmap、endian、overflow |
| 4 | Stack/Queue/Heap | Min stack + 手写 heap + 阻塞队列读代码 |
| 5 | Graph | BFS/DFS + topo；讲清邻接表内存 |
| 6 | C++ 对象模型 | Rule of Five、virtual、layout 跑 examples 01–08 |
| 7 | 复盘 | 默写 SharedPtr 骨架 + 一张 list/tree 题限时 |

### Week B — System Design 手撕

| Day | 主题 | 动作 |
|-----|------|------|
| 8 | Queue + Ring | 阻塞队列 + SPSC；讲 memory_order |
| 9 | Pool + Allocator | object_pool + two_level_mempool + ABA |
| 10 | LRU + Rate limiter | raw-list LRU + token bucket |
| 11 | Thread pool + CV | doc 07；死锁与公平性 |
| 12 | Lock-free / Cache | MPMC 或 fixed pool；false sharing |
| 13 | 综合 Mock | 限时 45min：Memory pool **或** Thread-safe LRU |
| 14 | 综合 Mock | 限时 45min：Thread pool **或** Rate limiter + 追问 |

---

## 5. 白板口述模板（System Design）

```
1. Clarify：单线程还是多线程？容量？延迟 vs 吞吐？能否用锁？
2. API：allocate/free 或 push/pop 签名；错误码
3. 数据结构：画图（free list / ring slots / hash+DLL）
4. 正确性：空/满、ABA、泄漏、异常安全
5. 并发：锁粒度 or CAS + memory_order
6. 性能：cache line、批量、零拷贝
7. 扩展：多核 Per-Core Cache、NUMA（有余力再说）
```

---

## 6. 与本仓其它文档的关系

| 文档 | 用途 |
|------|------|
| [08-高频问答速查.md](./08-高频问答速查.md) | 语言 FAQ 一页纸 |
| [12-EC2-Nitro-Coding高频题.md](./12-EC2-Nitro-Coding高频题.md) | 电话面 Coding 专项 |
| 本文 | **题型全景 checklist** + 手撕映射 + 两周日程 |
| [../README.md](../README.md) | 18 Part 深度讲义入口 |
