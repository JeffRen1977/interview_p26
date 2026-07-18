# 设计高性能异步事件循环：多传感器嵌入式 Runtime

> **核心考点：** Reactor + **每传感器一条 SPSC 无锁环**；主动轮询 vs 休眠省电；零拷贝描述符；多模态时间戳对齐  
> **场景：** 智能眼镜 / 嵌入式设备 — IMU 200Hz、Camera 30Hz、Mic/I2C 更低，要求低延迟 + 低 CPU  
> **关联 Demo：** [sensor_event_loop.py](./sensor_event_loop.py) · [sensor_event_loop.cpp](./sensor_event_loop.cpp)  
> **关联手撕：** [../interview_handwrite/spsc_ring_buffer.cpp](../interview_handwrite/spsc_ring_buffer.cpp) · [../docs/24-无锁SPSC队列与Cacheline对齐.md](../docs/24-无锁SPSC队列与Cacheline对齐.md) · [realtime-voice-assistant.md](./realtime-voice-assistant.md)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 假设 | 设计影响 |
|------|------|----------|
| 设备 | AR 眼镜 / 边缘 SoC，电池供电 | 空闲必须能 sleep，不能 busy-spin 耗电 |
| 传感器 | IMU 200Hz 小包；Camera 30Hz 大帧；Mic/I2C 更低 | **异速多源**，不能一刀切批处理 |
| SLA | IMU→控制/姿态 &lt; 5ms；Camera→Agent 可几十 ms | 热路径禁锁、禁 malloc |
| 内存 | 几十～几百 MB 可用 | 预分配 ring + 帧池；视频只传指针 |
| OS | Linux（epoll）或 RTOS（event flag） | 同一 Reactor 抽象，wait 原语不同 |
| 失败 | 队列满怎么办 | **丢最旧或丢最新**，绝不在 IRQ/采集路径 `realloc` |

**开场金句：**

> 这不是「开几个线程各读各的传感器」。Embedded Experiences 的题眼是：**在异速多源输入下，用 SPSC 把采集与推理解耦，用单线程 Event Loop 保证确定顺序，再用 poll/sleep 在延迟和功耗之间切换。**

---

## 1. 总体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                     嵌入式多传感器事件循环系统                          │
└────────────────────────────────────────────────────────────────────────┘

 [ 硬件/内核层 ]         [ 边缘 Runtime 无锁缓冲 ]          [ Event Loop 核 ]

 ┌──────────┐            ┌──────────────────────┐
 │ IMU IRQ  ├───────────►│ SPSC Ring (小包)     ├──────┐
 └──────────┘            └──────────────────────┘      │
                                                       ▼
 ┌──────────┐            ┌──────────────────────┐   ┌─────────────┐
 │ Camera   ├───────────►│ SPSC Ring (帧描述符) ├──►│ Event Loop  │
 │ V4L2/DMA └───────────►│ 只存 ptr / dma-buf   │   │ (绑核)      │
 └──────────┘            └──────────────────────┘   └──────┬──────┘
                                                       ▲   │ 唤醒/投递
 ┌──────────┐            ┌──────────────────────┐      │   ▼
 │ Mic/ALSA ├───────────►│ SPSC Ring            ├──────┘ ┌─────────────┐
 └──────────┘            └──────────────────────┘        │ Agent Core  │
                                                         │ 多模态推理  │
                                                         └─────────────┘
```

**三层分工：**

| 层 | 职责 | 禁止事项 |
|----|------|----------|
| I/O / IRQ / DMA | 采数、填描述符、push 本通道 SPSC | 拿全局锁、做重推理、动态扩容 |
| Event Loop | 按优先级 drain 各环、时间对齐、唤醒策略 | 阻塞式磁盘/网络（可丢给 worker） |
| Agent Core | 融合后的多模态输入推理 | 直接碰硬件寄存器 |

---

## 2. 为什么「每传感器一条 SPSC」，而不是一个大 MPMC 队列

| 方案 | 问题 |
|------|------|
| 所有传感器 → 一把 mutex 队列 | 锁竞争 + 优先级反转；IMU 被 Camera 拷贝拖慢 |
| 单条 MPMC 无锁环 | 多生产者 CAS → Cache Line Bouncing；嵌入式上不值 |
| **每源一条 SPSC** | 写入者唯一（该传感器采集线程/IRQ bottom half），读取者唯一（Event Loop）→ **无 CAS，仅 acquire/release** |

视频通道的元素是 **描述符**（`dma_iova` / `fd` / `byte_len`），不是整帧像素 → 零拷贝意向。

---

## 3. I/O 接入：epoll / 中断 + 采集线程

```
Linux:
  Camera → V4L2 dqbuf → 得到 dma-buf fd → push Camera SPSC
  Mic    → ALSA/period → push Audio SPSC
  IMU    → SPI/I2C IRQ 或 input 子系统 → push IMU SPSC

统一 wait：
  Event Loop 侧用 epoll 等「有数据」信号（eventfd / 队列水位），
  而不是在 epoll 里直接读大帧（读帧留在采集路径）。
```

**要点：** epoll 管的是 **就绪通知**；重活仍在各自 producer。Event Loop 被唤醒后去 **无锁 pop** 各 SPSC。

---

## 4. Event Loop：Active Poll vs Sleep

```
while running:
    processed = false
    while imu_q.pop():      process_imu();   processed = true   # 高优先级先排空
    while cam_q.pop():      process_cam();   processed = true
    while audio_q.pop():    process_audio(); processed = true

    if processed:
        # 刚忙过：可短自旋再看一眼（降低唤醒延迟）
        continue
    else:
        # 全空：epoll_wait / futex 休眠 → 省电
        wait_for_wakeup()
```

| 模式 | 何时 | 收益 |
|------|------|------|
| **Active Poll** | 近期持续有 IMU/帧 | 无挂起延迟，适合 200Hz |
| **Block/Sleep** | 连续空转 N μs | CPU idle → 眼镜续航 |

**绑核：** Event Loop 绑大核 / 隔离核，避免与 UI/散热线程抢时间片。

**唤醒：** producer `push` 成功后对 `eventfd` write(1) 或 `notify`；注意 **lost wakeup**（先 push 再检查「loop 是否在 wait」的经典竞态 → eventfd 计数或「先设 pending 再 push」）。

---

## 5. 白板级 SPSC（C++ 意向）

完整可运行版见仓库 SPSC 手撕；这里强调面试要说清的语义：

```cpp
// Capacity 必须 2^k；用 & (Cap-1) 代替 %
// head / tail 单调递增；index = counter & mask
// Producer 只写 tail；Consumer 只写 head
// push: 写 slot → release-store tail
// pop:  acquire-load tail → 读 slot → release-store head
// alignas(64) 隔离 head/tail，消灭 False Sharing
```

**满队列策略（必主动说）：**

| 策略 | 适用 |
|------|------|
| Drop newest（push 失败） | 保护旧上下文；实现最简单 |
| Drop oldest（覆盖 head） | 视觉/IMU 更在意「最新样本」 |
| 背压减采样 | Camera：降帧；IMU：降 ODR |

**禁止：** 在 IRQ / 实时采集路径 `new` / `realloc` 扩环。

---

## 6. 多模态时间戳对齐（高频追问）

IMU 200Hz（~5ms 一拍），Camera 30Hz（~33ms）。Agent 要的是：

```
Camera frame @ T_c  +  IMU samples in [T_c - Δ, T_c + Δ]
```

**Event Loop 侧维护 IMU 环形历史（可与 SPSC 分离的固定 ring）：**

1. 每 pop 到 IMU → append 到 `imu_history`（带 `timestamp`）  
2. 每 pop 到 Camera → 在 history 上 **二分 / 双指针** 取 $\pm 2.5\text{ms}$（或 $\pm$ 半个 IMU 周期）样本  
3. 打包 `MultimodalInput{frame_desc, imu_slice}` → Agent

```
imu_history (按时间有序)
  ... | t=100 | t=105 | t=110 | t=115 | ...
Camera @ t=112  → 取 [109.5, 114.5] 内 IMU → 常为 1～2 个样本
```

时钟：尽量用 **同一硬件时钟域**（SoC monotonic / PTP）；跨芯片要做 offset 估计，面试提一句即可。

---

## 7. 功耗与热

| 手段 | 作用 |
|------|------|
| 空闲 `epoll_wait` | 释放 CPU |
| 动态批处理 | Camera 积压时一次 drain 多帧，但设上限防尾延迟 |
| 传感器 ODR 降级 | Agent 休眠时 IMU 200→50Hz |
| NPU 推理与 Event Loop 分离 | 长推理不堵 IMU drain（可丢给高优 worker，结果再回环） |

---

## 8. 面试追问速答

### Q1：为什么 `alignas(64)` head/tail？

> False Sharing：同 Cache Line 上 producer 写 `tail`、consumer 写 `head` 会互相 Invalid。拆到两行后两边可并行持有。

### Q2：队列爆了怎么办？

> 定长环 + Drop oldest/newest；绝不实时路径扩容。监控 `drop_count`，联动降采样。

### Q3：IMU 与 Camera 如何 sync？

> Event Loop 内滑动窗口 / 有序 history + 二分；以 Camera 时间为锚捞 IMU。

### Q4：为何不用「一个线程一个传感器直接调 Agent」？

> Agent 多模态需要 **同一时间基上的融合**；多线程直接推推理 → 锁、乱序、重复跑模型。单 Loop 是顺序与背压的控制点。

### Q5：和 Proactor / io_uring 的关系？

> Linux 上采集可用 io_uring/V4L2；对 Event Loop 仍是「完成事件入 SPSC」。模式上更接近 **Reactor：就绪后由 Loop 取数据**；DMA 完成中断填环则带一点 Proactor 味道。面试说清「通知在核、数据在环」即可。

### Q6：10x 传感器数量？

> 每源仍 SPSC；Loop 可用 **优先级分层**（IMU 每 tick 必扫，Camera 限额）；或多 Loop 按 SoC 簇拆分，Agent 前再汇合。瓶颈常在内存带宽与 NPU，不是 epoll 本身。

---

## 9. 口述 60 秒收尾

> 每类传感器一条 SPSC：采集线程只生产、Event Loop 只消费，无 CAS。视频环里只放 dma-buf 描述符做零拷贝。Loop 绑核：有数据就优先排空 IMU，再处理帧；连续空闲则 epoll/futex 睡省电，push 后 eventfd 唤醒。队列满丢样本不扩容。Camera 到达时在 IMU history 里按时间戳窗口对齐，再交给 Agent。整条热路径无锁、无 malloc、可测 drop 与 p99 延迟。

---

## 10. Demo

```bash
cd openai
python3 sensor_event_loop.py

cmake -S . -B build && cmake --build build --target sensor_event_loop
./build/sensor_event_loop
```

演示：多速率假传感器 → 每源 SPSC → 单线程 Loop drain → Camera 触发时对齐最近 IMU。
C++ 版额外展示真实 `acquire`/`release` 原子语义与 `condition_variable` 休眠唤醒。
