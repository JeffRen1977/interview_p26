# 设计智能眼镜多模态 Agent Runtime（超低功耗 / 端云协同）

> **核心考点：** Thermal & Battery Limits → Always-On 级联唤醒；Memory Wall → dma-buf 零拷贝 + Block KV；Hybrid Router → 本地 NPU vs 云端 LMM  
> **场景：** Smart Glasses / Snapdragon AR1·AR2 — 整机约 **1.5–2.5W**、**2–4GB** 共享 LPDDR、多模态 TTFT **&lt;1s**  
> **关联：** [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) · [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) · [realtime-voice-assistant.md](./realtime-voice-assistant.md) · [../docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md)  
> **手撕对照：** [sensor_event_loop.cpp](./sensor_event_loop.cpp) · [../interview_handwrite/spsc_ring_buffer.cpp](../interview_handwrite/spsc_ring_buffer.cpp) · [../interview_handwrite/two_level_mempool.cpp](../interview_handwrite/two_level_mempool.cpp)

---

## 0. 澄清需求（Ambiguity）

传统云端设计理念在眼镜上失效。必须先向面试官主动钉死三条物理边界：

| 维度 | 假设 | 设计影响 |
|------|------|----------|
| 功耗 / 散热 | 整机 **1.5–2.5W**；贴脸不能烫 | 主 SOC 不能 Always-On；DSP 级联唤醒 |
| 电池 | **400–600mAh** ≈ 2–3h 持续多模态 | 空闲路径 &lt;10mW；推理路径短促 burst |
| RAM | SoC **2–4GB LPDDR5**，CPU/GPU/NPU 共享 | 禁动态扩 KV；Block 预分配 + LRU 回收 |
| 延迟 | Wake Word **&lt;100ms**（纯本地）；VLM TTFT **0.5–1s** | 热路径零拷贝、禁止堆分配 |
| 网络 | 弱网 / 离线常见 | Hybrid：RTT 高强制本地；复杂意图才上云 |

**开场金句：**

> Embedded Experiences 在眼镜上不是「把云端 Runtime 缩小」。架构中心是 **在 Thermal / Memory Wall / Battery 三个物理边界内做决策**：何时唤醒主 SOC、什么在 NPU 跑、什么上云、什么永远不出设备。

---

## 1. 软件运行架构（端-云分层 Hybrid）

```
 ┌─────────────────────────────────────────────────────────────────────────┐
 │                      智能眼镜多模态 Agent 运行时系统                      │
 └─────────────────────────────────────────────────────────────────────────┘

   [ 传感器采集层 ]             [ 本地极低功耗处理层 (Always-On Subsystem) ]

   ┌─────────────┐             ┌────────────────────────────────┐
   │ 麦克风 (I2S)├────────────►│ 1. 硬件 VAD / 唤醒检测 (DSP)   │
   └─────────────┘             └────────────────┬───────────────┘
                                                │
   ┌─────────────┐             ┌────────────────▼───────────────┐
   │ 摄像头 (MIPI)├────────────►│ 2. 视觉变化检测 (Motion Detect)│
   └─────────────┘             └────────────────┬───────────────┘
                                                │
                                                │ (触发唤醒 / 状态机改变)
                                                ▼
                               [ 端侧 AI 推理层 (Main SOC / NPU / GPU) ]

                               ┌────────────────────────────────┐
                               │ 3. 边缘端 Agent 调度器 (C++)     │
                               │    - 零拷贝 环形缓存队列         │
                               │    - 动态 KV Cache 缓存管理器  │
                               └────────────────┬───────────────┘
                                                │
                               ┌────────────────▼───────────────┐
                               │ 4. 本地端模型链 (ExecuTorch)   │
                               │    - Whisper-tiny (INT8)       │
                               │    - Mini-VLM / LLaVA (INT4)   │
                               └────────────────┬───────────────┘
                                                │
                                                ▼ (低带宽 / 超长上下文 / 复杂工具)
                               [ 混合网络路由层 (Cloud Offloading) ]

                               ┌────────────────────────────────┐
                               │ 5. 远端高性能推理 (云端 LMM)    │
                               └────────────────────────────────┘
```

与 [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) 的衔接：第 3 层调度器用 **每传感器一条 SPSC** + 单线程 Event Loop；本篇侧重 **Always-On 状态机、模型链、Hybrid 路由**。

---

## 2. 极致省电：Always-On 状态机与级联唤醒

主 SOC 不能长期 Active。底层设计 **多层级联唤醒**：

### 2.1 第一层：Always-On Audio（&lt;10mW DSP）

- 主 SOC 处于 Deep Sleep。
- 音频经 I2S/PDM 持续流入独立微型 DSP。
- DSP 上跑极轻量 **硬件 VAD + Wake-Word**。
- 仅检测到有效唤醒后，DSP 发 **IRQ** 唤醒主 SOC 进入全功率路径。

### 2.2 第二层：Always-On Vision（低分辨率 + Motion）

- 「静止」时不以高清长图采集；用极低分辨率（如 **320×240 @ 5fps**）。
- 超低功耗协处理器做 **Motion Detection**。
- 画面显著变化，或用户主动触发（双击镜腿 → **IMU 中断**）时，才拉高 MIPI 时钟，唤醒主传感器抓高画质帧。

**面试金句：** 省电不是「算得快一点」，是 **尽量不让主 SOC 醒来**；醒来之后再谈 NPU 吞吐。

---

## 3. 极致低延迟：零拷贝、量化、KV、投机解码

主 SOC 唤醒后，Runtime 优化必须到微秒级。

### 3.1 传感器 → 推理引擎：dma-buf 零拷贝

| 步骤 | 做法 |
|------|------|
| 采集 | Camera 驱动写入 CMA 物理连续区 |
| 传递 | 不 `memcpy`；把 **dma-buf FD** 以引用交给 NPU Backend Delegate（ExecuTorch） |
| 效果 | 1080p 多模态输入搬移从 **30–50ms → ~0ms** |

对照语音链路中的同一模式：[realtime-voice-assistant.md](./realtime-voice-assistant.md) § 异构流水线。

### 3.2 端侧量化

| 模型 | 量化 | 理由 |
|------|------|------|
| Whisper-tiny（ASR） | **INT8** | 语义不失真；贴合 NPU 8-bit 吞吐 |
| Mini-VLM / LLaVA-3B | **INT4 Group-wise** + 关键激活 **FP16** | 权重压内存带宽；激活保精度 |

### 3.3 KV Cache：Block 静态预分配 + LRU

端侧 **禁止** `new`/`malloc` 动态增长 KV（碎片 + OOM）。

- 初始化一次性占满可用 NPU/共享内存：**固定大小 Block 池**
- Runtime 用无锁环管理空闲 Block（对照 [two_level_mempool.cpp](../interview_handwrite/two_level_mempool.cpp)）
- 生成新 Token 时，对无用上下文做 **LRU 回收**（PagedAttention 思想搬到边缘）

> 面试金句：边缘一旦有多轮对话 / 时序，**KV 内存**就是一等瓶颈；PagedAttention 从服务端搬到眼镜同样成立。详见 [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) 与 [../LLM/paged_attention.py](../LLM/paged_attention.py)。

### 3.4 Speculative Decoding（异构算力）

- **草稿模型**（100–200M）在 CPU/GPU 高频产 draft tokens
- **目标 VLM**（~3B）在 NPU 一次并行验证
- 端侧 decode 吞吐常见 **1.5×–2×**（视草稿命中率）

---

## 4. 端-云协同动态路由（Hybrid Decision Router）

并非所有请求本地跑，也非所有请求适合上云。Runtime 内放一层 **轻量启发式路由**：

```cpp
enum class InferenceRoute {
    LOCAL_NPU,   // 纯本地：时间查询、拍照录制、简单实体识别
    CLOUD_LMM    // 云端：长篇翻译、复杂逻辑推理、超长上下文
};

InferenceRoute make_routing_decision(const std::string& intent, double current_rtt) {
    // 1. 网络：RTT 过高或断网 → 强制本地
    if (current_rtt > 300.0) {
        return InferenceRoute::LOCAL_NPU;
    }
    // 2. 意图：超轻量端侧分类器 / 规则集
    if (intent == "complex_reasoning" || intent == "long_translation") {
        return InferenceRoute::CLOUD_LMM;
    }
    return InferenceRoute::LOCAL_NPU;
}
```

| 路由 | 典型意图 | 约束 |
|------|----------|------|
| `LOCAL_NPU` | wake 后短问答、实体识别、离线 | TTFT 预算内；不烧带宽 |
| `CLOUD_LMM` | 复杂推理、长翻译、工具链 | 需 RTT 可接受；可降级回本地小模型 |

与相机舰队题的差异：相机题用 **置信度级联 + Embedding 上云** 控成本；眼镜题用 **意图 + RTT + 电量** 控延迟与离线可用性。哲学相同：**在约束下做路由决策**。

---

## 5. 面试官高频追问

### Q1：多模态（图像 + 音频）如何解决时间戳漂移？

**要点：**

1. **硬件级同步**：SOC PWM 作 Camera V-Sync；Mic 驱动与同一系统参考时钟（如 `CLOCK_MONOTONIC` / 硬件 timer）打戳。
2. 数据进入无锁 SPSC 后，**单线程 Event Loop** 在同一物理时间窗口对齐（详见 [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md)）。
3. 软件补偿只做兜底；面试优先讲 **硬件 time-stamping**，再讲软件对齐窗口。

### Q2：NPU 被更高优先级任务（如 AR 几何解算）占满怎么办？

**要点：**

1. 预先编译 **热备份后端**：同一量化模型具备 NPU / GPU / CPU Delegate。
2. Runtime 监控 NPU 负载或抢占信号 → 瞬时 **failover / 分流** 到 GPU/CPU。
3. 用户侧交互不卡顿优先于「必须跑在 NPU」；可降草稿长度、关 speculative、缩短 max tokens。

### Q3：为什么眼镜不能照搬云端 Prefill/Decode 调度？

**要点：** Prefill 吃带宽与功耗 burst；Decode 吃 KV 与长时发热。眼镜上要 **短 burst + 快睡**，配合 Always-On 级联，而不是常驻大 batch 吞吐优化。

### Q4：400mAh 下如何估多模态会话时长？

**粗算话术：** Always-On DSP ~10mW 可待机数日量级；每次唤醒主 SOC + NPU 按 1.5–2W × 会话秒数累加。设计目标是 **提高唤醒阈值、缩短每次 Active 窗口**，而不是一味堆模型。

---

## 6. 面试口述 3 分钟版

1. **钉约束：** 1.5–2.5W、2–4GB 共享内存、Wake &lt;100ms、VLM TTFT &lt;1s。  
2. **Always-On：** DSP VAD/Wake + 低分辨率 Motion；IRQ 才唤醒主 SOC。  
3. **热路径：** dma-buf 零拷贝 → ExecuTorch INT8 ASR + INT4 VLM；Block KV + LRU。  
4. **加速：** 小模型草稿 + NPU 验证的 Speculative Decoding。  
5. **Hybrid：** 意图 + RTT 路由；弱网强制本地；复杂推理上云。  
6. **追问预埋：** 硬件时间戳对齐；NPU 抢占时 GPU/CPU failover。

---

## 7. 自测清单

- [ ] 为什么主 SOC 不能 Always-On？第一层唤醒功耗量级是多少？  
- [ ] 1080p 进 NPU 为什么能做到 ~0ms 搬移？dma-buf / CMA 各自角色？  
- [ ] 端侧 KV 为什么必须 Block 预分配？和 PagedAttention 什么关系？  
- [ ] Hybrid Router 的输入特征有哪些（intent / RTT / 电量 / 热状态）？  
- [ ] 多传感器时间对齐：硬件方案 vs 软件窗口各讲什么？  
- [ ] NPU 被 AR 任务抢占时的降级路径是什么？  
- [ ] 与「10 万相机混合路由」题的相同点与不同点？
