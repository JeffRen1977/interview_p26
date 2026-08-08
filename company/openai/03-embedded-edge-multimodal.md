# 03 - 嵌入式、多模态与边缘端落地（Embedded Experiences）

> **最契合 Embedded Experiences 团队** — 软硬件协同、弱网、低算力、低延迟

---

## Q5. 实时智能语音助手端到端架构（硬件化 Advanced Voice Mode）

> **完整标准答案（300ms 预算、WebRTC/FEC、Barge-in+AEC、dma-buf）：**  
> [realtime-voice-assistant.md](./realtime-voice-assistant.md) · Demo：[voice_assistant_pipeline.py](./voice_assistant_pipeline.py)

### 5.1 SLA

| 指标 | 目标 |
|------|------|
| 端到端对话延迟 | **< 300ms**（用户停说到听到首个音节） |
| 弱网 | 5% 丢包下可懂 |
| Barge-in | 用户开口 **< 50ms** 内停止播放 |

### 5.2 全链路架构

```
┌─────────────── Device (Embedded) ───────────────┐
│ Mic ──► HW AEC ──► VAD ──► Opus encode        │
│         │              │                       │
│         │              └── Barge-in trigger    │
│ Speaker ◄── Opus decode ◄── jitter buffer    │
│         ▲                                      │
│    NPU: wake word / optional on-device ASR    │
└─────────┼──────────────────────────────────────┘
          │ WebRTC (UDP + SRTP + FEC)
          ▼
┌─────────────── Cloud ───────────────────────────┐
│ SFU / TURN ──► Streaming ASR (partial tokens)   │
│              ──► LLM (streaming text)           │
│              ──► TTS (streaming audio chunks)   │
└─────────────────────────────────────────────────┘
```

### 5.3 为什么 WebRTC / UDP，而不是 TCP

| | TCP | WebRTC/UDP |
|---|-----|------------|
| 丢包 | 重传 → 延迟抖动 | FEC / PLC，优先实时性 |
| 头部开销 | 可靠但 head-of-line blocking | 适合 20ms 音频帧 |
| 移动网络 | 切换基站时 TCP 窗口崩溃 | ICE 重连 + 快速 resume |

**FEC：** 每 N 个 audio packet 发 1 个 parity；丢包无需重传即可恢复。

### 5.4 Barge-in 机制

1. **端侧 VAD**（硬件或 NPU）检测用户 speech onset
2. 立即：**停止本地播放** + 清空 jitter buffer
3. 发送 **CancelStream** 控制消息（SCTP data channel 或 RTCP APP packet）
4. 云端：**abort LLM/TTS**，KV cache 回滚到 last committed turn
5. **AEC（回声消除）** 必须在触发 VAD 前收敛，否则误检 TTS 回声

### 5.5 异构流水线与零拷贝

```
Camera/Mic DMA buffer (dmabuf fd)
    │
    ├─► NPU inference (import dmabuf, no memcpy)
    └─► CPU preprocessing (optional, mmap same fd)
```

- Linux **`dma-buf`** 在驱动、NPU、GPU 间共享物理页
- 嵌入式面试加分：讲 **ION/DMA-BUF heap**、cache coherency（SMP vs 设备 coherent）

### 5.6 上下文与状态

| 状态 | 存放 |
|------|------|
| 短期对话 | 云端 KV Cache + session_id sticky |
| 长期记忆 | 向量库（异步写入，不阻塞 300ms 路径） |
| 端侧 | 仅 wake word + 最小 session token |

### 5.7 10x Scaling Drill

> 「1 万并发语音会话时 SFU 带宽 primeiro；100 万时 **TTS GPU 与 LLM Decode** 瓶颈。降级：短回复模式、on-device 小模型处理简单 intent、非关键 turn 异步 TTS。」

---

## Q6. 10 万台智能相机的混合路由与端云协同

> **完整标准答案（INT4 端侧、PagedAttention、置信度级联、Embedding 上云）：**  
> [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) · Demo：[hybrid_sensor_routing.py](./hybrid_sensor_routing.py)

### 6.1 约束

- 10 万设备 × 1080p@15fps → 云端不可承受全量上传
- 目标：检测异常事件，误报率 < X%，云端成本 <$Y/设备/月

### 6.2 多级路由（Hybrid Routing）

```
Camera Frame
    │
    ▼
Edge NPU: 1B–3B INT4 小模型
    ├─ object detect / motion / scene classify
    ├─ PagedAttention 本地 KV（见 LLM/paged_attention.py）
    └─ confidence score C
           │
           ├─ C >= θ_high ──► 本地告警 + 元数据上报（无原图）
           ├─ θ_low <= C < θ_high ──► 上传 embedding + 低分辨率 crop
           └─ C < θ_low ──► 丢弃（或本地录制 ring buffer 待查）
           │
           ▼ (cloud trigger)
Cloud: gpt-4o / 大 VLM
    ├─ 复杂推理、长文本 OCR、多物体关系
    └─ 反馈 fine-tune 阈值（federated stats，非 raw video）
```

### 6.3 带宽与成本优化

| 数据类型 | 大小 | 何时上传 |
|----------|------|----------|
| 事件 JSON | ~KB | 总是 |
| Embedding vector | ~KB | 中置信度 |
| 720p clip 5s | ~MB | 低置信度 + 用户订阅 |
| 1080p 全流 | 禁止默认 | 仅人工复核抽样 |

### 6.4 控制平面

- **Device Shadow / Digital Twin** — 期望模型版本、阈值 θ
- **OTA** — A/B 模型灰度；回滚
- **Command Queue** — MQTT/QUIC，下行「提高灵敏度 24h」

### 6.5 安全与隐私

- 端侧 **on-device encryption**；云端仅处理脱敏 embedding
- 租户隔离：camera_id → tenant namespace

### 6.6 Embedded Experiences 话术

> 「Embedded 不是缩小版云端 — 是 **在约束下做决策路由**：什么在 NPU 算、什么上云、什么永远不出设备。我会用 confidence-calibrated cascade 把云端调用降到 <5%。」

---

## Q7. 多传感器异步事件循环（眼镜 / 边缘 Runtime）

> **完整标准答案（每源 SPSC、poll/sleep 省电、IMU↔Camera 时间对齐）：**  
> [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) · Demo：[sensor_event_loop.py](./sensor_event_loop.py) / [sensor_event_loop.cpp](./sensor_event_loop.cpp)

异速输入（IMU 200Hz / Camera 30Hz）下，用 **Reactor + 每传感器一条 SPSC** 解耦采集与 Agent；热路径 Active Poll，空闲休眠省电；视频环只传 dma-buf 描述符。

---

## Q8. 智能眼镜多模态 Agent Runtime（功耗 / 内存墙 / 端云协同）

> **完整标准答案（Always-On 级联唤醒、dma-buf 零拷贝、Block KV、Hybrid 路由）：**  
> [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md)

### 8.1 三条物理边界（开场先钉死）

| 边界 | 量级 | 设计后果 |
|------|------|----------|
| Thermal / Power | 整机 **1.5–2.5W**；电池 **400–600mAh** | 主 SOC Deep Sleep；DSP Always-On |
| Memory Wall | **2–4GB** 共享 LPDDR | 禁动态扩 KV；Block 预分配 |
| Latency | Wake **&lt;100ms**；VLM TTFT **0.5–1s** | 零拷贝 + INT4/INT8 + 短 Active 窗口 |

### 8.2 分层架构一句话

Always-On DSP（VAD/Wake + Motion）→ IRQ 唤醒 → C++ Agent 调度（SPSC + KV）→ ExecuTorch 本地模型链 → 意图/RTT **Hybrid Router** 决定是否上云。

### 8.3 Embedded Experiences 话术

> 「眼镜 Runtime 的一等公民是 **不唤醒**。唤醒之后才是 dma-buf、INT4 VLM 和 Hybrid：简单意图本地闭环，复杂推理才上云，弱网强制本地。」

---

## Q9. 边缘协同 Agent Runtime（弱网 / 断网 / 最终一致性）

> **完整标准答案（QUIC+Outbox、CRDT、动态 Quorum、WAL、Graceful Degradation）：**  
> [edge-collaborative-agent-runtime.md](./edge-collaborative-agent-runtime.md)

### 9.1 与 Q8 的边界

| | Q8 智能眼镜 | Q9 边缘协同 |
|--|-------------|-------------|
| 范围 | 单机功耗 / 端云 | 多机分区 / 一致性 |
| 一等公民 | 不唤醒主 SOC | Outbox + 离线自治 |
| 关键坑 | 内存墙 / TTFT | 物理执行器双脑 |

### 9.2 一致性分层一句话

感知与意图走 **CRDT 最终一致**；主控选举走 **Local Raft + Dynamic Quorum**；物理执行器走 **Lease**，禁止 LWW。

### 9.3 Embedded Experiences 话术

> 「弱网协同不是强行 Raft 全员在线。先 Outbox 扛断连，再按状态类型选 CRDT 或 Quorum；执行器用租约，重连用 Delta+QoS 防洪峰。」

---

## 关联代码与文档

- [LLM/](../LLM/) — KV Cache、PagedAttention、FlashAttention
- [docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md) — INT4 量化、NPU
- [docs/05-系统设计题与模拟面试.md](../docs/05-系统设计题与模拟面试.md) — capture-to-display 延迟预算
- [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) — 多传感器 Event Loop
- [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) — 智能眼镜 Agent Runtime
- [edge-collaborative-agent-runtime.md](./edge-collaborative-agent-runtime.md) — 边缘协同 Agent Runtime
