# 设计实时智能语音助手端到端架构（硬件化 Advanced Voice）

> **岗位契合：** OpenAI **Embedded Experiences**  
> **核心 SLA：** 用户停说 → 听到回复首音节 **&lt; 300ms**；弱网可懂；Barge-in **&lt; 50ms** 停播  
> **关联 Demo：** [voice_assistant_pipeline.py](./voice_assistant_pipeline.py)  
> **关联：** [streaming-chatgpt-backend.md](./streaming-chatgpt-backend.md)（cancel / KV 回滚）· [../LLM/kv_cache.py](../LLM/kv_cache.py)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 假设 | 设计影响 |
|------|------|----------|
| 延迟定义 | **口到耳**：endpoint → first audible TTS | 全链路预算表，不是「云端 TTFT」 alone |
| 网络 | 4G/5G/Wi‑Fi，1–5% 丢包，抖动大 | **禁 TCP 音频**；WebRTC+FEC |
| 设备 | 低功耗 SoC + NPU，RAM 紧 | 端侧只做 AEC/VAD/编码；大模型上云 |
| 交互 | 可随时打断 | 端侧 VAD 主导 barge-in + 云端上下文回滚 |
| 多模态 | 可选摄像头 | dma-buf 异构零拷贝流水线 |

**开场金句：**

> 300ms 是 **物理实时预算**，不是普通 HTTP API SLA。移动网丢包下 TCP 重传会直接打爆预算，所以音频面必须 **UDP/WebRTC + FEC**。端侧用硬件 AEC 保证 VAD 不把扬声器回声当用户说话，Barge-in 要在本地先停播，再让云端 **按 commit_cursor 回滚 Context**。

---

## 1. 300ms 延迟预算（先画表再画架构）

定义：**用户端点（endpoint）→ 扬声器播出第一个 TTS 音节**。

| 阶段 | 预算 | 手段 |
|------|------|------|
| 端侧 AEC + VAD + Opus 编码 | 20–30ms | 硬件 AEC；20ms 帧 |
| 上行网络（WebRTC） | 30–50ms | UDP；近区 PoP；FEC |
| Streaming ASR（partial） | 40–80ms | 流式 ASR，不必等整句 |
| LLM 首 token / 首语义 | 60–100ms | sticky KV；小模型路由；投机解码 |
| Streaming TTS 首包音频 | 40–60ms | 流式 TTS；勿等全文 |
| 下行网络 + jitter buffer | 30–50ms | 小 jitter（牺牲完美清晰换延迟） |
| **合计** | **≈ 220–370ms** | 压各段；超标则砍 jitter / 用端侧小意图模型 |

> 面试金句：预算是 **零和** 的——jitter buffer 加 60ms「更稳」往往会直接违反 300ms。

**并行流水（关键）：**

```
ASR partial "what's the wea…" ──► 已可启动 LLM（投机）
LLM token 流 ──► 边生成边 TTS（勿等 [DONE]）
```

串行「ASR 完成 → LLM 全文 → TTS 全文」必然 &gt; 300ms。

---

## 2. 全链路架构

```
┌──────────────────────── Device (Embedded) ────────────────────────┐
│                                                                    │
│  Mic ──► HW AEC ◄── playback ref ── Speaker                        │
│            │                                                       │
│            ▼                                                       │
│           VAD ──── barge-in ──► stop decode + flush jitter         │
│            │              └──► CancelStream (DataChannel)          │
│            ▼                                                       │
│        Opus encode                                                 │
│            │                                                       │
│  Cam ──► dma-buf ──► NPU (wake word / optional VAD)                │
│            └──► CPU (零拷贝 mmap) 可选                             │
└────────────────────────────┬───────────────────────────────────────┘
                             │ WebRTC: UDP + SRTP + FEC + NACK(limited)
                             ▼
┌──────────────────────── Cloud ─────────────────────────────────────┐
│  Edge SFU / TURN                                                   │
│       │                                                            │
│       ▼                                                            │
│  Streaming ASR ──partial text──► LLM (stream) ──text──► TTS stream │
│       │                         │ cancel/rollback                  │
│       └─────────────────────────┴──── session + KV sticky worker   │
└────────────────────────────────────────────────────────────────────┘
```

---

## 3. 为什么音频面摒弃 TCP，改用 WebRTC / UDP + FEC

### 3.1 TCP 在实时语音上的死法

| 问题 | 后果 |
|------|------|
| **丢包重传** | RTT 级停顿；口到耳延迟尖刺 |
| **队头阻塞** | 丢 1 包挡住后续已到包 |
| 拥塞窗口 | 基站切换 / Wi‑Fi 漫游时吞吐塌方 |
| 长连接中间件 | 企业防火墙对长 TCP 不友好 |

音频要的是 **按时到达可播的帧**，不是 100% 可靠字节流。

### 3.2 WebRTC 选型

| 组件 | 作用 |
|------|------|
| **ICE / STUN / TURN** | 穿越 NAT；最坏走 TURN 中继 |
| **SRTP** | 加密媒体 |
| **Opus @ 20ms** | 低延迟编码；丢包隐藏（PLC） |
| **DataChannel (SCTP)** | Barge-in / cancel 控制面 |
| **FEC / NACK** | 抗丢包；NACK 要严格限次以免拖延迟 |

### 3.3 前向纠错（FEC）

```
每 k 个媒体包 → 生成 1 个 parity（或 Opus in-band FEC）
丢包 ≤ 可纠能力 → 本地恢复，不重传
```

| 策略 | 延迟 | 带宽 | 适用 |
|------|------|------|------|
| 重传 (NACK) | 高（+RTT） | 低 | 可容忍延迟的分享 |
| **FEC** | 低 | 高 10–30% | **实时对话** |
| PLC | 最低 | 0 | 次要补偿 |

**弱网策略：** 上调 FEC；**下调 jitter buffer**；必要时降码率。绝不默默改回 TCP。

---

## 4. Barge-in：VAD + 硬件 AEC + 云端 Context 回滚

### 4.1 为什么必须先有 AEC

扬声器播放 TTS 时，麦克风会收录 **回声**。若无 AEC：

- VAD 把 TTS 当作用户说话 → **误打断**  
- 或用户真说话被回声淹没 → **漏打断**

**硬件 / 固件 AEC：** 用 playback reference 信号自适应滤波，在进 VAD 前去掉回声。AEC 收敛前（起播数十 ms）可短暂提高 VAD 阈值。

### 4.2 端侧打断时序（&lt; 50ms 停播）

```
t=0     VAD=speech_onset（已过 AEC）
t=0+    ① 立即本地播放（硬切）
        ② 清空 jitter buffer / 丢弃在途 RTP
        ③ DataChannel: CancelStream{
             session_id, generation_id, commit_cursor, media_ts
           }
t&lt;50ms  用户已听不到助手声音
```

> **本地停播不能等云端 ACK**——云端往返可能就 &gt; 50ms。

### 4.3 云端处理与 Context 精准回滚

与文本流式 cancel 同构（见 streaming backend）：

```
commit_cursor = 客户端已「对用户生效」的位置
  - 文本: 已展示 token index
  - 语音: 已播放的 TTS 时间戳 / 已合成且 ack 的 chunk id

Cancel 到达:
  1. abort ASR tail / LLM decode / TTS 合成
  2. 释放 GPU batch slot + KV 超出 commit 的部分
  3. conversation state 回到 last committed user/assistant turn
  4. 开启新的 listening generation_id
```

**竞态：**

| 竞态 | 处理 |
|------|------|
| Cancel 路上仍有 TTS 包到达 | 端侧已 flush；丢弃 `generation_id` 不匹配的包 |
| 误 VAD | 短 hangover；能量+过零率+NPU 分类；可云端确认 |
| 双通道（蓝牙）AEC 难 | 尽量走内置 DSP；外设降级策略 |

---

## 5. 异构流水线与 dma-buf 零拷贝

### 5.1 问题

经典路径：

```
Driver → copy → userspace → copy → NPU input → copy → CPU post
```

在低功耗设备上，**拷贝与 cache 维护**会吃掉 300ms 预算和电量。

### 5.2 dma-buf 零拷贝

```
Mic/Camera DMA
    │
    ▼
dma-buf fd  ──────────────────────────────┐
    │                                     │
    ├─► NPU runtime: import fd（设备地址）│  无 memcpy
    ├─► GPU/DSP: 同 fd                    │
    └─► CPU: mmap(fd) 只读触达            │
```

| 要点 | 说明 |
|------|------|
| **单块物理缓冲** | 多消费者共享；引用计数 |
| **Cache coherency** | CPU 读前 `sync_for_cpu`；设备读前 `sync_for_device` |
| **Heap** | ION / DMA-BUF heaps（carveout / system） |
| **流水线** | 多缓冲（3–4 个 buf）重叠：采集∥推理∥编码 |

### 5.3 端侧角色划分

| 模块 | 跑在哪 | 原因 |
|------|--------|------|
| AEC | HW DSP | 硬实时、省 CPU |
| Wake word / VAD | NPU | 持续监听低功耗 |
| Opus | CPU/DSP | 标准编解码 |
| LLM / 大 ASR / TTS | 云 | 算力不够；保 300ms 只传音频 |

---

## 6. 会话状态与 Model-Aware

| 状态 | 位置 | 说明 |
|------|------|------|
| 短期对话 KV | 云端 sticky GPU worker | 多轮不重复 Prefill |
| commit_cursor | 端 + 云 | Barge-in 真理来源 |
| 长期记忆 | 异步向量库 | **禁止**挡在 300ms 路径上 |
| 端侧 | session token + 音频环形缓冲 | 断线可本地提示 |

弱网降级：

1. 降码率 + 增 FEC  
2. 端侧小模型处理「停 / 继续 / 音量」等命令  
3. 云端改短答模式（少 token → 少 TTS）

---

## 7. 观测指标

| 指标 | 目标 |
|------|------|
| mouth_to_ear_ms P50/P99 | &lt; 300 / &lt; 500 |
| barge_in_stop_ms | &lt; 50 |
| fec_recover_rate / loss_rate | 弱网可懂 |
| vad_false_barge_rate | 低误打断 |
| cancel_to_gpu_free_ms | 及时释放算力 |

---

## 8. 10x / 100x Scaling Drill

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1× | 单区 ASR/TTS GPU | 流式流水线；分区 |
| 10× | SFU / TURN 带宽 | 就近接入；按会话分片 |
| 100× | LLM Decode + TTS | 短答；端侧意图过滤；非 VIP 降模 |

> 「设备侧 300ms 预算在 100x 时仍然成立；崩的是云端 GPU。端侧 Barge-in 与 cancel 反而更重要——快速释放错误生成。」

---

## 9. 面试口述 3 分钟版

1. **预算表：** 口到耳 300ms，ASR∥LLM∥TTS 流水，禁止全文串行。  
2. **网络：** 音频走 WebRTC/UDP+FEC；TCP 重传不可接受。  
3. **Barge-in：** HW AEC → VAD → **本地先停播** → CancelStream → 云端按 commit_cursor 回滚 KV。  
4. **零拷贝：** dma-buf 让 Mic/NPU/CPU 共享缓冲，省拷贝与电量。  
5. **降级：** 弱网加 FEC、减 jitter；简单 command 端侧搞定。

---

## 10. 参考实现

```bash
cd openai
python3 voice_assistant_pipeline.py
```

模拟：延迟预算校验、FEC 抗丢包、AEC 后 VAD barge-in、commit_cursor 回滚、dma-buf 式零拷贝计数。

---

## 11. 追问清单（自测）

- [ ] 300ms 从哪个事件量到哪个事件？  
- [ ] 为什么抖动缓冲不能随意加大？  
- [ ] FEC 与 NACK 如何分工？  
- [ ] 无 AEC 时 VAD 会怎样？  
- [ ] 为什么停播不能等云端 ACK？  
- [ ] commit_cursor 在语音里对应什么？  
- [ ] dma-buf 的 cache sync 为什么必要？  
- [ ] 蓝牙耳机场景 AEC 怎么降级？
