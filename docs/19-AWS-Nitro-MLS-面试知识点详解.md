# 19 - AWS EC2 Nitro MLS 面试知识点详解

> 面向 **EC2 Nitro MLS**（机器学习超算平台）Senior SDE 岗。  
> 本文把 JD 提示中的每个考点**展开到可面试口述的深度**，并链接仓库内已有材料。  
> **配套：** [17-系统设计](./17-AWS-EC2-Nitro-系统设计.md) | [amazon_cpp](../amazon_cpp/) | [interview_handwrite/cpp](../interview_handwrite/cpp/)

---

## 总览：四轮面试考什么

| 轮次 | 权重 | 考什么 |
|------|------|--------|
| **Coding** | 高 | LeetCode Medium + C++ 并发 DS（LRU、Blocking Queue） |
| **底层 C++** | 极高 | 内存模型、Move、智能指针、虚函数、STL 原理 |
| **System Design** | 极高（L6） | ML 集群调度、EFA/SRD、分布式训练、Checkpoint |
| **Behavioral (LP)** | 一票否决 | Customer Obsession、Dive Deep、Ownership、Deliver Results |

---

# 第一部分：系统编程与底层架构（C/C++ / Rust）

## 1.1 Linux 内存模型与虚拟内存

### 必会概念

```
用户进程看到的虚拟地址 (VA)
        ↓  MMU + 页表 (Page Table)
物理地址 (PA) → DRAM / 设备 MMIO
```

| 概念 | 面试怎么说 |
|------|-----------|
| **虚拟内存** | 每个进程独立地址空间；内核通过页表把 VA 映射到 PA；支持隔离、按需分配、swap |
| **页 (Page)** | 通常 4 KB；分配、保护、换出的基本单位 |
| **页表** | 多级页表（x86-64 通常 4 级）节省内存；TLB 缓存近期翻译，**TLB miss 很贵** |
| **Huge Pages** | 2 MB / 1 GB 大页；减少页表项和 TLB miss |
| **mmap** | 把文件或匿名内存映射进地址空间；共享内存、零拷贝的基础 |

### Huge Pages 对大模型训练的影响（MLS 必答）

**问题：** 大模型训练分配数十 GB 连续内存，4 KB 页导致：
- 页表占用大（内存开销）
- TLB miss 频繁 → 内存访问延迟抖动

**方案：**
```bash
# 预留大页（需 root）
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# 应用侧
mmap(..., MAP_HUGETLB | MAP_ANONYMOUS, ...);
# 或 transparent huge pages (THP)，内核自动合并，但可能碎片化
```

**面试答法：**
> 训练 workload 是内存带宽 bound。Huge pages 降低 TLB miss，让 GPU/CPU 的 DMA 和 host pinned memory 更稳定。需要与 `cudaHostAlloc` / `numactl` 配合，在正确的 NUMA 节点上分配。

### 高频问答

**Q: 进程和线程共享什么内存？**
- 同一进程内线程共享代码段、堆、全局数据；**各自有独立栈和寄存器**
- 写共享数据需要同步（mutex / atomic）

**Q: Copy-on-Write (CoW) 在哪用？**
- `fork()` 后父子共享页表项，写时才复制页
- `mmap(MAP_PRIVATE)` 写时复制

**Q: 什么是 page fault？**
- 缺页：合法 VA 未映射 → 内核分配物理页
- 非法访问 → SIGSEGV
- 性能调优时要区分 major fault（读盘）vs minor fault

**深入学习：** [amazon_cpp/docs/02-内存指针与布局.md](../amazon_cpp/docs/02-内存指针与布局.md)

---

## 1.2 并发：无锁、死锁、内存屏障、False Sharing

### 1.2.1 锁 vs 无锁

| | Mutex | Lock-free | Wait-free |
|---|-------|-----------|-----------|
| 定义 | 阻塞等待 | 系统整体有进展 | 每个线程有限步完成 |
| 适用 | 临界区较长、逻辑复杂 | 热路径计数器、SPSC 队列 | 极少需要 |
| 风险 | 死锁、优先级反转 | ABA、复杂正确性证明 | 实现极难 |

**无锁不等于更快** — 高竞争下 CAS 重试可能更慢。MLS 场景：Nitro mailbox、metrics 计数器、per-core 统计适合 lock-free；复杂状态机仍用 mutex。

### 1.2.2 死锁四个必要条件（以及如何打破）

1. **互斥** — 资源独占  
2. **持有并等待** — 拿着 A 等 B  
3. **不可抢占** — 不能强行夺走  
4. **循环等待** — A→B→C→A  

**预防：**
- 固定加锁顺序：`std::lock(m1, m2)` + `lock_guard` adopt_lock
- 超时：`try_lock_for`
- 缩小临界区；用无锁结构减少锁

**手撕：** [interview_handwrite/cpp/bounded_blocking_queue.cpp](../interview_handwrite/cpp/bounded_blocking_queue.cpp)

### 1.2.3 Memory Barrier / Memory Order

**编译器重排 + CPU 乱序执行** 导致多线程看不到「直觉顺序」。

```cpp
// 发布-订阅模式
data = 42;
ready.store(true, std::memory_order_release);  // 之前的写不能重排到此后

// 消费者
while (!ready.load(std::memory_order_acquire)) {}
assert(data == 42);  // 保证看到 data=42
```

| order | 语义 | 场景 |
|-------|------|------|
| `relaxed` | 原子性，无同步 | 纯计数 |
| `acquire` | 读端：此后不重排到此前 | consumer |
| `release` | 写端：此前不重排到此后 | producer |
| `seq_cst` | 全局顺序（默认） | 简单正确，略慢 |

**SPSC 无锁队列：** [interview_handwrite/cpp/spsc_ring_buffer.cpp](../interview_handwrite/cpp/spsc_ring_buffer.cpp)

### 1.2.4 False Sharing（伪共享）

两个核写**同一 cache line（64B）** 的不同变量 → line 来回失效 → 性能暴跌。

```cpp
// 差
struct Bad { std::atomic<int> a; std::atomic<int> b; };

// 好：padding 到不同 cache line
struct alignas(64) Padded { std::atomic<int> a; char pad[60]; };
struct alignas(64) Padded { std::atomic<int> b; char pad[60]; };
```

**MLS 场景：** 多 GPU 训练节点的 per-rank 统计、NCCL 进度计数、Host Agent heartbeat。

**深入学习：** [amazon_cpp/docs/06-并发与内存模型.md](../amazon_cpp/docs/06-并发与内存模型.md) | [examples/10_concurrency_atomic.cpp](../amazon_cpp/examples/10_concurrency_atomic.cpp)

---

## 1.3 C++ 移动语义、RVO、智能指针

### std::move 底层

```cpp
std::move(x)  // = static_cast<T&&>(x)，只做类型转换
```

实际「移动」发生在**移动构造/赋值**被调用时：窃取指针，原对象置空。

### 完美转发

```cpp
template<typename T>
void wrapper(T&& arg) {
    work(std::forward<T>(arg));  // 保持 lvalue/rvalue 类别
}
```

用于工厂函数、`emplace_back`、避免多余拷贝。

### RVO / NRVO

```cpp
Buffer make() { return Buffer(1024); }  // C++17 保证省略拷贝/移动
```

面试：**优先依赖 RVO**；移动语义是优化后备，不是设计依赖。

### shared_ptr 引用计数开销

- 大小 16B（ptr + control block）
- `fetch_add` / `fetch_sub` 原子操作 — **热路径有开销**
- 默认首选 `unique_ptr`；共享所有权才用 `shared_ptr`
- `make_shared` 一次分配，更 cache-friendly

**深入学习：** [amazon_cpp/examples/01_rule_of_five.cpp](../amazon_cpp/examples/01_rule_of_five.cpp) | [05_move_forwarding.cpp](../amazon_cpp/examples/05_move_forwarding.cpp) | [06_smart_pointers.cpp](../amazon_cpp/examples/06_smart_pointers.cpp)

---

## 1.4 Rust 所有权与零拷贝（若简历写了 Rust）

| C++ 概念 | Rust 对应 |
|----------|-----------|
| RAII | 所有权 + Drop trait |
| unique_ptr | 默认唯一所有权 |
| shared_ptr | `Arc<T>` |
| 数据竞争 | 编译期拒绝（borrow checker） |
| 零拷贝 | `&[u8]` slice 传递、`mmap` + `bytes` crate |

**面试答法：**
> Rust 在编译期保证内存安全和线程安全，适合写 Nitro Host Agent、驱动接口等长期运行的底层服务。零拷贝通过借用语义传递 buffer 视图，避免堆分配；与 C 互调用用 FFI + 明确生命周期文档。

---

## 1.5 PCIe、DMA、RDMA、CXL

### PCIe 基础

```
CPU ←→ PCIe Root Complex ←→ Switch ←→ Endpoint (GPU/NIC/NVMe/Nitro Card)
```

| 概念 | 说明 |
|------|------|
| **BAR** | Base Address Register；设备寄存器和内存窗口映射到 Host VA |
| **DMA** | 设备直接读写 Host 内存，不经 CPU 拷贝 |
| **MSI-X** | 消息 signaled 中断，比传统 IRQ 更可扩展 |
| **带宽** | Gen4 x16 ≈ 32 GB/s；Gen5 翻倍；实际受协议开销影响 |

**面试画图：** Host 内存 → DMA descriptor ring → 设备拉数据 → 完成中断/ doorbell

### RDMA (Remote Direct Memory Access)

- NIC 直接读写**远端**机器内存
- 绕过远端 CPU 和内核协议栈
- **InfiniBand** 和 **RoCE**（RDMA over Converged Ethernet）是训练集群主流
- AWS **EFA** 在此基础上提供 HPC 级网络（见第二部分）

### CXL (Compute Express Link)

- 基于 PCIe 物理层，提供**缓存一致性**的内存扩展
- 类型：CXL.mem（扩展内存）、CXL.cache（设备缓存主机内存）、CXL.io（标准 IO）
- **MLS 相关性：** 未来大内存池、池化内存、异构计算互联；面试可说「了解趋势，训练集群当前仍以 HBM + NVLink/EFA 为主」

---

## 1.6 内核态/用户态、中断、高效 I/O

### 上下文切换开销

| 路径 | 开销来源 |
|------|----------|
| 系统调用 | 用户态→内核态切换、参数拷贝、内核逻辑 |
| 中断 | 硬件中断 → 内核 ISR → 可能唤醒用户线程 |
| 传统 read/write | 数据经过内核 buffer 拷贝到用户空间 |

### 零拷贝技术

| 技术 | 原理 | 场景 |
|------|------|------|
| **mmap** | 文件/设备映射到用户地址空间，直接访问 | 大文件、共享内存 |
| **sendfile** | 内核直接把文件页发送到 socket | 静态文件服务 |
| **splice** | 管道间零拷贝搬运 | proxy |
| **DMA + pinned memory** | GPU/NIC 直接访问物理连续内存 | ML 训练 |
| **io_uring** | 共享提交/完成队列，批量异步 I/O，减少 syscall | 高 QPS 存储/网络 |

### epoll vs io_uring

| | epoll | io_uring |
|---|-------|----------|
| 模型 | 事件通知，每次仍需 syscall（epoll_wait） | 共享 ring buffer，可批量、可 polling |
| 成熟度 | 极高 | Linux 5.1+，生态快速成长 |
| MLS | 控制面 API 服务 | 高吞吐数据面（若做存储/日志 agent） |

**面试答法：**
> Nitro data plane 不走 Host Agent 转发；控制面 Agent 用 gRPC/epoll 足够。若做 checkpoint 流式写 NVMe/S3，可考虑 io_uring 批量提交降低 syscall 开销。

---

## 1.7 NUMA 与亲和性绑定

### NUMA 架构

```
Node 0: CPU0-31 + Local DRAM + GPU0,1
Node 1: CPU32-63 + Local DRAM + GPU2,3
跨 Node 访问内存：延迟高、带宽低
```

**优化命令：**
```bash
numactl --cpunodebind=0 --membind=0 ./train.py
nvidia-smi topo -m   # 查看 GPU-GPU、GPU-CPU NUMA 拓扑
```

**MLS 训练 checklist：**
- GPU 对应 CPU core 绑在同一 NUMA node
- `cudaSetDevice` 后用对应 node 的 pinned memory
- NCCL 拓扑感知：同机 NVLink > PCIe > 跨机 EFA

---

# 第二部分：ML Infra 系统设计

> 完整白板题见 [17-AWS-EC2-Nitro-系统设计.md](./17-AWS-EC2-Nitro-系统设计.md)

## 2.1 AWS EFA 与 SRD

### 为什么标准 TCP/IP 不够？

大模型分布式训练通信特点：
- **大量小消息 + 少量大消息**（AllReduce 桶）
- **尾部延迟敏感** — 最慢节点决定整体 step 时间（straggler）
- TCP 重传、拥塞控制、内核协议栈开销导致 **tail latency 高**

### EFA (Elastic Fabric Adapter)

- AWS 为 HPC/ML 提供的**专用网络接口**
- 绕过传统 TCP 栈，提供 **OS-bypass** 的用户态通信（类似 Libfabric/OFI）
- 与 **NCCL** 集成，用于跨节点 GPU 通信
- 用于 Trn1/Trn2、P4d/P5 等实例

### SRD (Scalable Reliable Datagram)

| | TCP | InfiniBand | SRD |
|---|-----|------------|-----|
| 连接 | 面向连接 | 可靠连接/数据报 | 多路径数据报 |
| 丢包 | 重传阻塞后续数据 | 链路层重传 | **多路径并行**，单路径丢包不阻塞全局 |
| 尾部延迟 | 队头阻塞 | 较低 | 针对 tail latency 优化 |
| 生态 | 通用 | HPC 传统 | AWS 内部 + NCCL |

**面试 30 秒版：**
> EFA 给 ML 实例提供 HPC 级 RDMA 能力。底层 SRD 用多路径路由把流量分散到多条网络路径，避免单点拥塞和 TCP 队头阻塞，从而降低 AllReduce 的 tail latency。这是 AWS 超大规模训练集群能 scale 到数千 GPU 的关键网络层之一。

---

## 2.2 集群互联拓扑

### Fat-Tree（胖树）

```
        Core switches
       /    |    \
   Agg switches ...
   /  |  \    /  |  \
  ToR ToR ToR ToR ...  → 每 rack 若干 GPU 节点
```

- **优点：** 任意两点间带宽可预测；易扩展
- **缺点：** 核心交换机成本高；收敛比设计需仔细
- **MLS：** 大规模训练集群常用，配合 rail-optimized 布局（同一 GPU index 跨机在同一 rail）

### Ring AllReduce

```
GPU0 → GPU1 → GPU2 → GPU3 → GPU0
  分片梯度沿 ring 传递并累加
```

- 带宽利用率高（尤其 8 GPU 单机 NVLink）
- 跨机时 ring 顺序影响延迟；NCCL 自动选 ring/tree

### 面试要画的图

```
[Node0: 8×GPU NVLink mesh] ←─EFA─→ [Node1: 8×GPU]
         ↑                                    ↑
    Local NVMe                          Local NVMe
         ↓                                    ↓
              S3 (checkpoint / dataset)
```

---

## 2.3 分布式训练并行与通信

### 三种并行

| 并行 | 切什么 | 主要通信 | 何时用 |
|------|--------|----------|--------|
| **Data Parallel (DP)** | batch | AllReduce 梯度 | 模型能放进单卡 |
| **Tensor Parallel (TP)** | 权重矩阵 | AllReduce/AllGather 每层 | 单层太大 |
| **Pipeline Parallel (PP)** | 模型层段 | P2P activation | 层数多 |

**FSDP / ZeRO：** 优化器状态分片，本质是 data parallel 的内存优化。

### 通信模式

```
AllReduce:  所有 rank 得到相同聚合结果（梯度平均）
AllGather:  收集各 rank 分片拼成完整张量
ReduceScatter: 聚合后分片返回
All-to-All:  MoE 专家路由
Broadcast:   广播参数（初始化）
```

### 计算与通信重叠（关键优化）

```
时间线（无重叠）:
  [Forward] [Backward] [AllReduce] [Optimizer]
  
时间线（有重叠）:
  [Forward]
  [Backward bucket3] [AllReduce bucket3]
  [Backward bucket2] [AllReduce bucket2]  ← 边算边传
  [Backward bucket1] [AllReduce bucket1]
  [Optimizer]
```

**实现：** PyTorch DDP `overlap_comm=True`；梯度分 bucket；NCCL 异步 stream

**面试数字举例：**
> 7B 模型 DP=64，每 step AllReduce ~14GB 梯度。若网络 200 Gbps EFA，理论 ~0.6s；重叠后可隐藏在 backward 的 ~70% 时间内，有效 step 时间接近纯计算。

---

## 2.4 高可用、Checkpoint、容错

### 规模下的现实

- 1000 GPU × 数周训练 → **平均每天数次节点故障** 是常态
- 故障来源：GPU ECC、NIC 错误、OOM、软件 bug、机架断电

### Checkpoint 设计

```
checkpoint/
  step_10000/
    model_shard_rank_{0..N}.pt
    optimizer_shard_rank_{0..N}.pt
    metadata.json   # parallel config, rng, dataloader offset
```

| 策略 | 优点 | 缺点 |
|------|------|------|
| **同步 checkpoint** | 一致性强 | 全员停顿 5–30 min |
| **异步 checkpoint** | 几乎不中断 | 可能丢最近几步 |
| **本地 NVMe 缓存 + 异步刷 S3** | 恢复快 | 节点全毁则依赖 S3 |

**减少 Stall time：**
1. 分片并行写（每 rank 写自己的 shard）
2. 写本地 NVMe（带宽 5–7 GB/s）再后台上传 S3
3. 增量 checkpoint（只写变化参数）
4. FP16/BF16 压缩

### 故障恢复流程

```
1. Health monitor 检测 GPU ECC / heartbeat 丢失
2. Scheduler 标记节点 DRAINING
3. 等待 in-flight step 完成或超时
4. 从 last successful checkpoint 重新 gang allocate
5. 并行拉 checkpoint 到 NVMe → 恢复训练
```

**详见：** [17 文档 Q2/Q3](./17-AWS-EC2-Nitro-系统设计.md)

---

# 第三部分：Amazon 领导力准则（LP）与 STAR 故事

## 3.1 准备方法

- 准备 **6–8 个深度技术项目故事**，每个可适配 2–3 条 LP
- 结构：**S**ituation → **T**ask → **A**ction → **R**esult（**必须有数字**）
- L6 强调：**你的技术决策**、**带人/影响团队**、**跨团队协调**

## 3.2 高频 LP 与技术故事映射

### Customer Obsession（顾客至上）

**面试官想听：** 你服务的「客户」是谁？怎么理解他们的痛点？技术优化如何转化为客户价值？

**故事模板（ML Infra 版）：**
> **S:** 内部 AI 平台团队反馈大模型训练 job 排队时间 p99 > 4h，影响 Anthropic/内部团队迭代速度。  
> **T:** 我负责训练集群调度器资源分配模块。  
> **A:** 分析发现 gang scheduling 因碎片率 40% 经常凑不齐节点；引入拓扑感知分配 + 软预留 + 15min 超时重试；与硬件团队对齐 rack 布局。  
> **R:** gang 一次成功率 62%→89%，p99 排队降至 45min；等效多支撑 30% job 吞吐，客户 SLA 达标。

**变体（Camera/端侧背景）：**
> 降低推理延迟 20% → 客户每 GPU 小时多跑 25% request → 直接降本。

### Dive Deep（刨根问底）

**面试官想听：** 复杂 bug /Debug 过程；工具链；从现象到 root cause 的推理链。

**故事模板：**
> **S:** 分布式训练 step 时间周期性抖动，p99 是 p50 的 3 倍。  
> **T:** 定位性能回归，发布窗口 2 周。  
> **A:**  
> 1. `nsys`/`ncu` 排除 GPU kernel 问题  
> 2. `perf record` 发现某 rank CPU 100%  
> 3. `strace` 发现频繁 `futex` 等待  
> 4. `gdb` + 源码定位 NCCL AllReduce 与 dataloader 抢锁  
> 5. 调整 dataloader `num_workers` + NUMA 绑定 + NCCL `NCCL_ASYNC_ERROR_HANDLING`  
> **R:** p99/p50 比从 3.0 降至 1.2；工具链：perf, strace, gdb, nsys, nccl-tests

**工具清单（面试可列举）：**

| 工具 | 用途 |
|------|------|
| `perf` / `flamegraph` | CPU 热点、cache miss |
| `bcc` / `eBPF` | 内核态追踪、网络延迟 |
| `gdb` / `lldb` | 用户态调试、core dump |
| `valgrind` / ASan | 内存泄漏、越界 |
| `nsys` / `ncu` | GPU timeline、kernel 分析 |
| `nccl-tests` | 网络带宽/延迟基线 |
| `nvidia-smi dmon` | GPU 利用率、ECC、温度 |

### Ownership & Invent and Simplify

**故事模板：**
> **S:** 新 ML 硬件 bring-up，固件/驱动/训练框架三方接口不一致，集成周期 6 周。  
> **T:** 作为 tech lead 定义 Host Agent 与 Nitro 通信抽象层。  
> **A:** 设计统一 mailbox API（cmd_id + 幂等 + 版本握手）；把 12 个 ad-hoc ioctl 收敛为 4 类命令；写仿真 mock 让框架团队无需真机即可开发。  
> **R:** 集成周期 6 周→2 周；后续 3 个新项目复用同一套 SDK。

### Deliver Results

**必须有量化 Impact：**

| 弱 | 强 |
|----|-----|
| 「优化了性能」 | 「DMA 传输带宽利用率 70%→93%」 |
| 「改善了稳定性」 | 「训练 job 因平台故障失败率 8%→1.2%」 |
| 「缩短了周期」 | 「checkpoint 恢复时间 45min→8min，训练有效利用率 +12%」 |

### 其他常考 LP

| LP | 故事方向 |
|----|----------|
| **Bias for Action** | 信息不全时先做 PoC 验证，而非无限设计评审 |
| **Have Backbone; Disagree and Commit** | 与硬件团队争论 DMA 方案；用数据说服；决策后全力执行 |
| **Earn Trust** | 跨时区协作；主动写 design doc；透明汇报风险 |
| **Hire and Develop the Best** | Mentor 2 名 junior 做并发模块；code review 文化 |

---

# 第四部分：ML 框架与编译器（加分项）

## 4.1 PyTorch 执行路径

```
Python nn.Module
    ↓ torch.compile (Dynamo)  [可选]
    ↓ TorchInductor / XLA / TensorRT backend
    ↓ 融合 kernel / 自定义算子
    ↓ GPU/Trainium/Inferentia 执行
```

### torch.compile / Dynamo

- **Dynamo** 捕获 Python 级别的计算图（FX Graph）
- 做算子融合、常量折叠、内存规划
- 后端可选 Inductor（Triton kernel）、XLA、TensorRT

**面试答法：**
> `torch.compile` 在 Python 层插桩，把动态图片段编译成静态子图，减少 kernel launch 开销和内存往返。在自定义加速器上可注册 backend，把子图 lower 到 Trainium/Inferentia 指令集。

## 4.2 XLA

- Google 的 **Accelerated Linear Algebra** 编译器
- 输入：HLO（High Level Operations）图
- 优化：算子融合、布局优化、内存分配、通信调度
- 用于 TPU；PyTorch/XLA 桥接 PyTorch 到 XLA

```
[conv] → [bias] → [relu]   ──XLA fuse──→  [fused_conv_bias_relu]
```

## 4.3 算子优化

### FlashAttention

**问题：** 标准 Attention $O(N^2)$ 内存（存完整 attention matrix）

**思路：** 分块计算 QK^T 和 softmax，**不物化完整矩阵**，在 SRAM 中流水

**影响：** 长序列训练/推理显存从 $O(N^2)$ 降到 $O(N)$，速度 2–4×

### Operator Fusion

| 融合 | 好处 |
|------|------|
| Conv + BN + ReLU | 少 2 次全局内存读写 |
| Linear + GELU | 减少 kernel launch |
| AllReduce + LayerNorm | 通信计算重叠（高级） |

**MLS 相关性：** Trainium/Inferentia 编译器（Neuron Compiler）大量依赖算子融合和静态图优化。

## 4.4 AWS 加速器栈（了解即可）

| 芯片 | 定位 | 软件栈 |
|------|------|--------|
| **Trainium** | 训练 | Neuron SDK, neuronx-distributed |
| **Inferentia** | 推理 | Neuron Runtime, 编译后模型 |
| **GPU (NVIDIA)** | 训练/推理 | CUDA, NCCL, cuDNN |

---

# 第五部分：Coding 与白板手撕

## 5.1 LeetCode（Medium 为主）

本仓库 [`leetcode/`](../leetcode/) 已覆盖高频题：

| 类型 | 题目 |
|------|------|
| 哈希 | two_sum |
| 设计 | lru_cache |
| 区间 | merge_intervals |
| 二分 | binary_search, search_rotated_array |
| 图 | number_of_islands |
| 堆 | top_k_frequent |
| 滑动窗口 | longest_substring_without_repeating |
| 链表/树 | reverse_linked_list, lowest_common_ancestor |

## 5.2 工程 DS（Amazon C++/Infra 极高频）

| 题目 | 路径 |
|------|------|
| Bounded Blocking Queue | [interview_handwrite/cpp/bounded_blocking_queue.cpp](../interview_handwrite/cpp/bounded_blocking_queue.cpp) |
| SPSC Ring Buffer | [interview_handwrite/cpp/spsc_ring_buffer.cpp](../interview_handwrite/cpp/spsc_ring_buffer.cpp) |
| LRU Cache | [interview_handwrite/cpp/lru_cache_ds.cpp](../interview_handwrite/cpp/lru_cache_ds.cpp) |
| Thread Pool | 见 [amazon_cpp/docs/07](../amazon_cpp/docs/07-Linux系统与设计题.md) 骨架 |
| shared_ptr 手写 | [nvidia/02-C++与嵌入式底层.md](../nvidia/02-C++与嵌入式底层.md) |

---

# 第六部分：面试前自查清单

## 硬核八股（逐项自测）

- [ ] 能画虚拟内存：VA → 页表 → PA，解释 TLB miss
- [ ] 能解释 Huge Pages 为何帮助大模型训练
- [ ] 能写 `mutex + condition_variable` 生产者消费者（**while 不是 if**）
- [ ] 能解释 `memory_order acquire/release` 发布订阅
- [ ] 能识别并修复 False Sharing（`alignas(64)`）
- [ ] 能解释 `std::move` vs 移动构造 vs RVO
- [ ] 能画 shared_ptr 控制块结构
- [ ] 能解释 vtable/vptr 和虚析构必要性
- [ ] 能解释 PCIe DMA 数据路径
- [ ] 能对比 epoll vs io_uring（一句话）
- [ ] 能解释 NUMA 绑定命令和 GPU 拓扑
- [ ] 能解释 EFA/SRD 解决 tail latency 的思路
- [ ] 能画 Fat-Tree 和 Ring AllReduce
- [ ] 能解释 DP/TP/PP 通信模式和 overlap
- [ ] 能设计异步 checkpoint 到 NVMe + S3
- [ ] 能讲 FlashAttention 和算子融合原理

## STAR 故事（6 个）

- [ ] Customer Obsession — 性能/成本帮客户
- [ ] Dive Deep — Debug 工具链 + root cause
- [ ] Ownership — 端到端负责 + 简化架构
- [ ] Invent and Simplify — 抽象层/SDK
- [ ] Deliver Results — 量化数字
- [ ] Earn Trust / Have Backbone — 跨团队争议

## 4 周冲刺计划

| 周 | 任务 |
|----|------|
| **W1** | amazon_cpp Part 1–3 + 手撕 Queue/LRU；整理 3 个 STAR 故事 |
| **W2** | 并发/内存模型 + NUMA/PCIe/DMA 复习；17 文档系统设计 Q1/Q2 |
| **W3** | EFA/SRD、分布式训练、Checkpoint；17 文档 Q3/Q4；LeetCode 15 题 |
| **W4** | 模拟 45min 系统设计；STAR 模拟面试；C++ 白板限时练习 |

---

## 资源索引

| 主题 | 文档 |
|------|------|
| C++ 全栈 | [amazon_cpp/README.md](../amazon_cpp/README.md) |
| 系统设计 | [17-AWS-EC2-Nitro-系统设计.md](./17-AWS-EC2-Nitro-系统设计.md) |
| 具身智能（若岗位交叉） | [18-具身智能大模型进展与面试准备.md](./18-具身智能大模型进展与面试准备.md) |
| 手撕代码 | [interview_handwrite/cpp/](../interview_handwrite/cpp/) |
