# 20 - Trainium × Nitro MLS 硬核面试题库（逐题准备）

> **岗位画像：** 定制芯片（Trainium）× 云端大模型基础设施 × Nitro 网络/虚拟化卸载。  
> **目标：** 构建能与 NVIDIA DGX 竞争的 AI 超级计算机。  
> **面试风格：** 少刷 DP，多考 **系统级 C++、HW/SW 协同、集群通信、底层系统设计**。  
> **配套：** [19-知识点详解](./19-AWS-Nitro-MLS-面试知识点详解.md) | [17-系统设计](./17-AWS-EC2-Nitro-系统设计.md) | [amazon_cpp](../amazon_cpp/) | [interview_handwrite](../interview_handwrite/)

---

## 如何使用本文

每道题按固定结构准备：

```
1. 面试官想考什么
2. 30 秒版答案
3. 3 分钟深挖版（画图 + 数字 + trade-off）
4. 常见 Follow-up
5. 你的手撕/代码入口
```

---

# 模块 A：C/C++ 并发与数据面编程

## A1. 实现一个线程安全的生产者-消费者队列

**考什么：** `mutex` + `condition_variable`、虚假唤醒、RAII。

**30 秒版：**
> 一个 `mutex` 保护 `deque`，两个 `condition_variable`（`not_empty` / `not_full`）。`put` 满时 wait，`get` 空时 wait；**必须用 while 循环 wait**，不能用 if。

**3 分钟版：**

```cpp
void put(T item) {
    std::unique_lock lock(mtx);
    not_full.wait(lock, [&]{ return q.size() < cap; });
    q.push_back(std::move(item));
    not_empty.notify_one();
}
```

| 要点 | 说明 |
|------|------|
| `notify_one` vs `notify_all` | 通常 one 够用；多消费者等同一条件可 all |
| 超时版本 | `wait_for` + 返回 false |
| 性能 | 控制面够用；数据面热路径考虑无锁 |

**Follow-up：**
- 多生产者多消费者怎么办？→ 一个 mutex 仍正确；极高吞吐用 MPMC ring buffer（见 H3）
- 如何优雅 shutdown？→ `done` flag + `notify_all`

**代码：** [bounded_blocking_queue.cpp](../interview_handwrite/bounded_blocking_queue.cpp)

---

## A2. 实现 SPSC 无锁 Ring Buffer（数据面核心）

> **深度拆解（SPSC + Memory Model + Cacheline）：** [24-无锁SPSC队列与Cacheline对齐.md](./24-无锁SPSC队列与Cacheline对齐.md)

**考什么：** 是否真懂 lock-free、memory order、Nitro/Trainium mailbox 思维。

**30 秒版：**
> 容量 N+1 区分满/空。生产者只写 `tail`，消费者只写 `head`。用 `atomic` + `release/acquire` 保证可见性。O(1) push/pop。

**3 分钟版：**

```
     head (consumer only)     tail (producer only)
       ↓                         ↓
[ ][X][X][X][ ][ ]   circular buffer
```

```cpp
bool push(const T& v) {
    size_t t = tail.load(relaxed);
    size_t next = (t + 1) % size;
    if (next == head.load(acquire)) return false;  // full
    buf[t] = v;
    tail.store(next, release);
    return true;
}
```

**Follow-up：**
- 为什么 SPSC 可以无锁，MPMC 不行？→ 多写者同时改 tail 会丢数据；需 CAS 或 mutex
- 什么是 ABA？→ 指针值相同但内容已变；用 tagged pointer / epoch
- Nitro 场景？→ Host↔Card 命令队列、DMA descriptor ring

**代码：** [spsc_ring_buffer.cpp](../interview_handwrite/spsc_ring_buffer.cpp)

---

## A3. `std::atomic` 的 memory order 怎么选？

**考什么：** 并发内存模型，不是背八股。

| order | 何时用 |
|-------|--------|
| `relaxed` | 纯计数器，不与其他变量建立顺序 |
| `release` | 生产者写完数据后 publish |
| `acquire` | 消费者看到 flag 后读数据 |
| `acq_rel` | `fetch_add` 等 RMW |
| `seq_cst` | 默认；不确定时用 |

**经典发布-订阅：**
```cpp
data = 42;
ready.store(true, memory_order_release);
// consumer:
while (!ready.load(memory_order_acquire)) {}
use(data);  // 保证看到 42
```

**Follow-up：** 和 `volatile` 区别？→ `volatile` 不保证原子性和跨线程顺序；MMIO 用 volatile，线程同步用 atomic。

**代码：** [10_concurrency_atomic.cpp](../amazon_cpp/examples/10_concurrency_atomic.cpp)

---

## A4. False Sharing 是什么？怎么修？

**考什么：** 多核/cache line 意识，Trainium 集群 per-rank 统计常踩坑。

**30 秒版：**
> 两个核写同一 64B cache line 的不同变量 → line 来回失效。修复：`alignas(64)` 把热变量隔离到不同 cache line。

**Follow-up：** 和 True Sharing 区别？→ True sharing 是逻辑上共享同一变量；false sharing 是逻辑无关但物理同行。

---

## A5. 手写 LRU Cache

**考什么：** `unordered_map` + 双向链表，O(1) get/put。

**结构：**
```
hash map: key → list::iterator
list: MRU (front) ... LRU (back)
```

**Follow-up：** 线程安全版？→ 粗粒度 mutex 包一层；或分片锁。

**代码：** [lru_cache_ds.cpp](../interview_handwrite/lru_cache_ds.cpp) | [leetcode/lru_cache](../leetcode/lru_cache/)

---

# 模块 B：内存、DMA、零拷贝

## B1. 解释 mmap 与零拷贝

**考什么：** 用户态如何高效访问内存/设备/文件。

**30 秒版：**
> `mmap` 把文件或设备内存映射到进程地址空间，访问像数组一样，避免 `read/write` 的内核拷贝。DMA 用 pinned memory（物理地址固定）让设备直接读写。

**数据路径对比：**

```
传统 read:  磁盘 → 内核 buffer → 拷贝 → 用户 buffer
mmap:       磁盘 → page cache ←── 用户直接访问（缺页时映射）
DMA:        设备 ←──────────────→ Host pinned pages（不经 CPU 拷贝）
```

**Follow-up：**
- `MAP_SHARED` vs `MAP_PRIVATE`？
- 为什么 DMA 要 pinned memory？→ 设备要物理地址连续/固定，不能 page fault
- Huge pages 关系？→ 减少 TLB miss，大 buffer 常用

---

## B2. Huge Pages 如何帮助大模型训练？

**考什么：** 是否理解 TLB 瓶颈。

**答：**
> 训练分配数十 GB 权重/梯度/optimizer state。4KB 页 → 千万级页表项 → TLB miss 频繁 → 内存访问延迟抖动。2MB/1GB huge pages 大幅减少 TLB 压力。配合 `numactl --membind` 在正确 NUMA node 分配。

```bash
numactl --cpunodebind=0 --membind=0 ./train
```

---

## B3. DMA 完整流程（面试白板题）

**考什么：** HW/SW 协同核心。

```
1. Host 分配 pinned buffer，填充 TX 数据
2. 写 DMA descriptor ring（地址、长度、flags）
3. doorbell / MMIO 通知设备
4. 设备 DMA 引擎读/写 Host 内存
5. MSI-X 中断或 polling 完成队列
6. Host 回收 descriptor，处理完成回调
```

**Follow-up：**
- 中断 vs polling？→ 低延迟/高吞吐数据面常用 polling（DPDK 风格）；中断适合稀疏事件
- IOMMU 作用？→ 设备只能访问允许的物理地址，防恶意/bug DMA

---

# 模块 C：Linux 内核与驱动

## C1. 用户态和内核态如何交互？

**考什么：** ioctl、sysfs、字符设备基础。

| 机制 | 用途 |
|------|------|
| **syscall** | 通用入口（read/write/mmap） |
| **ioctl** | 设备特定命令（配置、触发操作） |
| **sysfs** | 暴露配置/状态（只读或读写） |
| **mmap** | 映射设备寄存器或 DMA buffer |
| **/dev/xxx** | 字符/块设备节点 |

**字符设备驱动骨架：**
```c
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
    .mmap    = my_mmap,
    .unlocked_ioctl = my_ioctl,
};
```

---

## C2. 中断处理：Top Half vs Bottom Half

**考什么：** 为什么不能在中断里干重活。

| 阶段 | 做什么 | 约束 |
|------|--------|------|
| **Top half (ISR)** | _ack 中断、读状态、调度 bottom half | 越快越好，不可阻塞 |
| **Bottom half** | 软中断 tasklet / workqueue / threaded IRQ | 可处理协议栈、唤醒用户线程 |

**Nitro/Trainium 场景：**
> 设备完成 DMA → MSI-X 中断 → ISR 标记完成 → workqueue 或 eventfd 唤醒用户态 polling 线程。

**Follow-up：** `threaded_irq`？→ 内核线程跑 handler，减少 top half 工作。

---

## C3. 设计 Host Agent 与加速器通信（系统设计）

**考什么：** control plane vs data plane 分离。

```
Control Plane (慢):  API → Host Agent → ioctl/mailbox → Firmware
Data Plane (快):     Training runtime → DMA/PCIe → Trainium（绕过 Agent）
```

**可靠性：**
- `cmd_id` + ACK，幂等命令
- 版本握手 `API_VERSION`
- 灰度升级 FW / Agent / Driver

**完整题解：** [17 文档 Q4](./17-AWS-EC2-Nitro-系统设计.md)

---

# 模块 D：HW/SW Co-design 与 Trainium 架构

## D1. CPU 如何向 Trainium/GPU 下发任务？

**30 秒版：**
> Host 把命令/描述符写入 **Command Queue（通常是 ring buffer）**，doorbell 通知设备；设备执行后写 **Completion Queue**，中断或 polling 通知 Host。

```
Host                          Accelerator
  │  write descriptors           │
  ├─────────────────────────────→│
  │  ring doorbell               │
  │                              │ execute
  │  ←───────────────────────────┤ completion entry
  │  poll / interrupt            │
```

**Follow-up：**
- 如何降低 CPU-Device 延迟？→ 批量提交、bigger batch、user-space driver、polling
- 和 CUDA stream 关系？→ stream 是逻辑队列，底层映射到 hardware queue

---

## D2. HBM vs SRAM vs DRAM

| 类型 | 带宽 | 容量 | 位置 |
|------|------|------|------|
| **SRAM** | 极高 | KB–MB | 芯片片上 |
| **HBM** | 很高 (~TB/s) | GB 级 | 与加速器封装 |
| **DDR DRAM** | 中等 | 大 | Host 主板 |

**Trainium 面试答法：**
> 矩阵乘是 memory-bound 时，算力再强也吃不饱；HBM 带宽决定 MFU 上限。编译器要做 tiling 让数据留在片上 SRAM/cache。

---

## D3. Compute-bound vs Memory-bound

**Roofline 模型（必会）：**

```
Performance
    │     ╱ compute roof (peak FLOPS)
    │    ╱
    │   ╱ memory roof (peak bandwidth × arithmetic intensity)
    │  ╱
    └────────────────── Arithmetic Intensity (FLOPs/byte)
```

| 类型 | 特征 | 优化方向 |
|------|------|----------|
| **Compute-bound** | 大矩阵、高 FLOPs/byte | 更高算力、更低精度 (BF16/FP8) |
| **Memory-bound** | 小矩阵、embedding、LayerNorm | 融合算子、增大 batch、HBM 带宽 |

**Follow-up：** 什么是 MFU (Model FLOPs Utilization)？→ 实际 FLOPs / 理论峰值 FLOPs；衡量芯片利用率。

---

## D4. Nitro 在 MLS 里的角色

**30 秒版：**
> Nitro 把网络、存储、安全管理从 Host CPU 卸载到专用 Nitro Card。MLS 团队在此基础上叠加 Trainium 超算能力——**数据面旁路 Host**，控制面由 Agent 管理。

---

# 模块 E：Scale-out 网络与集合通信

## E1. RDMA 和 RoCE 是什么？

**RDMA：** NIC 直接读写远端内存，绕过远端 CPU 和内核协议栈。

**RoCE：** RDMA over Converged Ethernet，在以太网上跑 RDMA。

**对比 TCP：**

| | TCP | RDMA/RoCE |
|---|-----|-----------|
| 拷贝次数 | 多次（内核↔用户） | 零拷贝 |
| CPU 参与 | 高 | 低 |
| 尾部延迟 | 队头阻塞 | 较低 |
| AWS | 通用 | **EFA**（增强版，SRD 多路径） |

---

## E2. EFA 和 SRD 解决什么问题？

**考什么：** AWS 差异化知识。

> 大模型 AllReduce 受 **tail latency** 影响。TCP 丢包/拥塞导致队头阻塞。EFA 提供 OS-bypass；SRD 用**多路径数据报**，单路径故障不阻塞全局，降低 p99 延迟。与 NCCL 深度集成。

---

## E3. 集合通信原语（必背）

| 原语 | 作用 | 典型场景 |
|------|------|----------|
| **Broadcast** | 一对多广播 | 广播初始权重 |
| **Reduce** | 多对一聚合 | 局部求和 |
| **AllReduce** | 聚合后每人都有一份 | **DP 梯度同步** |
| **AllGather** | 收集拼接 | TP 收集激活 |
| **ReduceScatter** | 聚合后分片 | ZeRO/FSDP |
| **All-to-All** | 全交换 | **MoE 专家路由** |

**Ring AllReduce 直觉：**
> N 个 GPU 排成环，梯度分 N 片，沿环传递并累加；带宽利用率高。

---

## E4. NCCL vs Neuron 分布式

| | NCCL (NVIDIA GPU) | neuronx-distributed (Trainium) |
|---|-------------------|-------------------------------|
| 硬件 | GPU + NVLink/EFA | Trainium + EFA |
| 拓扑 | 自动探测 ring/tree | 编译时 + runtime 通信库 |
| 集成 | PyTorch DDP/FSDP | PyTorch Neuron |

**面试答法：**
> 我不需要背 Neuron API 细节，但理解：框架发出 collective → runtime 选算法（ring/tree）→ 底层走 EFA/RDMA。优化点是拓扑感知和计算通信重叠。

---

## E5. 计算与通信重叠（Overlap）怎么设计？

**30 秒版：**
> Backward 按 bucket 切分；算完一个 bucket 的梯度就异步发起 AllReduce，与下一 bucket 的 backward 并行。

```
[Backward bucket3] [AllReduce bucket3]
[Backward bucket2] [AllReduce bucket2]  ← 时间重叠
[Backward bucket1] [AllReduce bucket1]
[Optimizer step]
```

**Follow-up：** 如何量化收益？→ 若 AllReduce 时间 < backward 时间，可完全隐藏。

**系统设计题：** [17 文档 Q3](./17-AWS-EC2-Nitro-系统设计.md)

---

# 模块 F：编译器、Neuron SDK、Lua

## F1. PyTorch 计算图如何到 Trainium？

> **完整编译运行时题解：** [23-Neuron编译运行时与数据面Lua.md](./23-Neuron编译运行时与数据面Lua.md)

```
PyTorch Model (Python)
    ↓ torch.jit / torch.compile / export
Computation Graph (FX / TorchScript)
    ↓ Neuron Compiler (或 XLA for JAX)
Optimized IR + 算子融合
    ↓
Trainium 机器码 / NEFF
    ↓
Neuron Runtime 执行
```

**关键优化：**
- **算子融合** — Conv+BN+ReLU 一次读写
- **静态 shape** — 编译期确定内存布局
- **通信调度** — 多芯片间 collective 插入点

---

## F2. XLA 做了什么？

> 接收 HLO 图 → 算子融合、内存分配、布局转换（NHWC/NCHW）、通信与计算调度 → 生成 TPU/Trainium 目标代码。

**面试举例：**
> 三个连续 elementwise op 融合成一个 kernel，减少 HBM 读写 3 次到 1 次。

---

## F3. FlashAttention 原理（加分）

**问题：** 标准 Attention 物化 $N \times N$ 矩阵 → $O(N^2)$ 显存。

**思路：** 分块计算 QK^T 和 softmax，在 SRAM 流水，**不存完整矩阵**。

**影响：** 长序列训练显存从 $O(N^2)$ 降到 $O(N)$。

---

## F4. 为什么数据面用 Lua？（JD 特别提到）

> **完整标准答案（PyTorch→NEFF、Lua C API、GC/FFI）：** [23-Neuron编译运行时与数据面Lua.md](./23-Neuron编译运行时与数据面Lua.md)

**30 秒版：**
> Lua 轻量、可嵌入 C/C++、LuaJIT 极快。适合**策略/路由/配置**等需要灵活又频繁执行的逻辑，不用每次改 C++ 重编译。

**典型场景：**
- 数据面包处理策略（类似 Nginx/OpenResty）
- 动态路由规则、A/B 配置
- 热更新控制逻辑而不重启二进制

**C 与 Lua 交互（Lua C API 核心）：**

```c
lua_State* L = luaL_newstate();
luaL_openlibs(L);

// 注册 C 函数给 Lua 调用
lua_register(L, "submit_dma", l_submit_dma);

// 加载脚本
luaL_dofile(L, "policy.lua");

// Lua 调 C：
// function route(pkt)
//   if pkt.size > 4096 then submit_dma(pkt) end
// end
```

| API | 作用 |
|-----|------|
| `lua_push*` / `lua_to*` | C↔Lua 栈传参 |
| `lua_register` | 注册 C 函数 |
| `lua_pcall` | 安全调用 Lua |
| `lua_getglobal` | 取 Lua 全局函数 |

**Follow-up：** 为什么不用 Python？→ GIL、启动慢、不适合 μs 级热路径；LuaJIT 更接近 C 性能。

**准备建议：** 读 《Programming in Lua》第 24–27 章（C API）；写 50 行 demo：C host 调 Lua 策略函数。

---

# 模块 G：系统设计白板题（三道必练）

## G1. 设计用户态高性能设备驱动（DPDK 风格）

> **完整标准答案（SQ/CQ/Doorbell/DMA/Polling）：** [21-Trainium-用户态数据面驱动架构.md](./21-Trainium-用户态数据面驱动架构.md)

### 需求澄清
- 目标延迟？< 10μs？
- 吞吐？百万 pps？
- 是否可多核 polling？

### 架构

```
┌─────────────────────────────────────┐
│  User-space Driver (no syscall hot path) │
│  ┌─────────┐  ┌──────────────────┐  │
│  │ Polling │→ │ Descriptor Ring  │  │
│  │ threads │  │ (pre-allocated)  │  │
│  └─────────┘  └──────────────────┘  │
│         ↕ mmap BAR / huge pages        │
└─────────────────────────────────────┘
         ↕ PCIe
    [Trainium / Nitro Card]
```

### 关键设计点

| 点 | 选择 |
|----|------|
| 中断 vs polling | 热路径 **polling**；冷路径中断 |
| 内存 | huge pages + pre-allocated pool |
| 多核 | 每核独立 queue pair，无锁 SPSC |
| 零拷贝 | 固定 buffer pool，描述符只传 index |
| 与内核共存 | UIO/vfio 映射 BAR；或 Nitro 已抽象好 |

### Trade-off

| 优 | 劣 |
|----|-----|
| 极低延迟、无 syscall | 占满 CPU core（busy poll） |
| 可预测性能 | 实现复杂、需处理设备复位 |

---

## G2. 设计多芯片参数同步（LLM 训练 Overlap）

> **完整标准答案（MFU、双缓冲、Chunking、EFA/SRD、Hardware Fence）：** [22-LLM训练计算通信重叠与MFU优化.md](./22-LLM训练计算通信重叠与MFU优化.md)

### 架构

```
┌──────── Trainium Node ────────┐
│ Chip0 Chip1 ... Chip7         │
│   ↕ NVLink/NeuronLink         │
│  Local AllReduce              │
└───────────────┬───────────────┘
                ↕ EFA (跨节点)
┌──────── Trainium Node ────────┐
│ Chip0 ... Chip7               │
└───────────────────────────────┘

Step pipeline:
  Forward → Backward (bucketed) → Async AllReduce per bucket → Optimizer
```

### 要点
1. **梯度分 bucket** — 与 backward 层对应
2. **双 stream** — 计算 stream + 通信 stream
3. **拓扑感知** — 同 rail GPU 优先通信
4. **故障检测** — NCCL timeout → 保存 checkpoint → 重调度

### 指标
- **MFU** 目标 > 40–55%（大模型典型）
- **通信占比** < 30% step time（重叠后）

---

## G3. 设计 MPMC 高吞吐 Ring Buffer

> **完整标准答案（Sequence + CAS + 描述符 + 退避）：** [25-无锁MPMC队列与CAS.md](./25-无锁MPMC队列与CAS.md)

### 需求
- 多生产者（数据采集线程）多消费者（DMA 提交线程）
- 百万级 ops/s，低延迟

### 方案对比

| 方案 | 实现 | 适用 |
|------|------|------|
| **Mutex ring** | 简单 | 中吞吐，面试先写这个 |
| **MPMC lock-free** | per-slot sequence number（如 Disruptor） | 高吞吐 |
| **SPSC 数组** | 每对生产者-消费者独立 ring | 最高效，可组合 |

### Lock-free MPMC 核心思路（Disruptor 模式）

```
每个 slot 有 sequence number:
  producer: CAS claim slot n, write, publish n
  consumer:  wait until slot n available, read, claim n+1
```

**面试策略：** 先写 **mutex 版** 证明正确，再讲如何升级 lock-free。

**代码入口：**
- Mutex 版：[thread_safe_ring_buffer.cpp](../interview_handwrite/thread_safe_ring_buffer.cpp)
- SPSC 版：[spsc_ring_buffer.cpp](../interview_handwrite/spsc_ring_buffer.cpp)

---

# 模块 H：模拟面试 Q&A 速查（20 题）

| # | 问题 | 要点 |
|---|------|------|
| 1 | new vs malloc | 构造、类型安全、异常 |
| 2 | std::move 移动了吗 | 只 cast，移动在构造/赋值 |
| 3 | 虚析构为什么必须 | 基类指针 delete 派生 |
| 4 | shared_ptr 线程安全吗 | refcount 原子；对象读写不安全 |
| 5 | mutex vs spinlock | 临界区长短 |
| 6 | condition_variable 为什么 while | 虚假唤醒 |
| 7 | 什么是 NUMA | 本地内存快；跨 node 慢 |
| 8 | PCIe DMA 流程 | descriptor → doorbell → 中断 |
| 9 | Huge pages 好处 | 少 TLB miss |
| 10 | epoll vs io_uring | 事件通知 vs 共享 ring |
| 11 | RDMA 是什么 | 远端内存直接访问 |
| 12 | EFA/SRD 价值 | 降 tail latency，多路径 |
| 13 | AllReduce 用途 | DP 梯度同步 |
| 14 | MFU 是什么 | 实际/峰值 FLOPs 利用率 |
| 15 | 计算通信重叠 | backward bucket + async AR |
| 16 | Checkpoint 如何减 stall | 异步写 NVMe + 分片 |
| 17 | control vs data plane | Agent 不转发训练流量 |
| 18 | Lua 为何在数据面 | 轻量嵌入、策略热更新 |
| 19 | FlashAttention | 分块不物化 N² 矩阵 |
| 20 | False sharing | alignas(64) 隔离 |

---

# 4 周备考计划（Trainium MLS 专用）

| 周 | 硬核技术 | 系统设计 | 手撕/代码 |
|----|----------|----------|-----------|
| **W1** | C++ 并发 + memory order | 读 17 文档 Q4 Host Agent | Queue + SPSC ring |
| **W2** | DMA/mmap/huge pages + 中断 | 17 文档 Q1 调度器 | LRU + object pool |
| **W3** | RDMA/EFA/NCCL/Neuron | 17 文档 Q3 分布式训练 | MPMC ring（mutex 版） |
| **W4** | Lua C API demo + Roofline | G1/G2/G3 白板模拟 | amazon_cpp 全真复习 |

---

# 面试当天 Checklist

- [ ] 带 3 个 STAR 故事（Dive Deep 必带 perf/ebpf 工具链）
- [ ] 能画：DMA 流程、双系统、Ring AllReduce、control/data plane
- [ ] 能写：blocking queue、SPSC ring（15 min 内）
- [ ] 能讲：EFA/SRD、MFU、overlap、Trainium 编译路径
- [ ] 准备 3 个问面试官的问题（团队最大挑战？Trainium bring-up 周期？）

---

## 资源索引

| 主题 | 路径 |
|------|------|
| 知识点展开 | [19-AWS-Nitro-MLS-面试知识点详解.md](./19-AWS-Nitro-MLS-面试知识点详解.md) |
| 系统设计 | [17-AWS-EC2-Nitro-系统设计.md](./17-AWS-EC2-Nitro-系统设计.md) |
| C++ 全套 | [amazon_cpp/README.md](../amazon_cpp/README.md) |
| 手撕代码 | [interview_handwrite/](../interview_handwrite/) |
| Neuron 官方 | [awsdocs-neuron.readthedocs.io](https://awsdocs-neuron.readthedocs.io/) |
| NVIDIA NCCL | [github.com/NVIDIA/nccl](https://github.com/NVIDIA/nccl) |
