# 设计支持实时流式传输的 ChatGPT 后端系统

> **规模假设：** 1 亿级注册用户 · 峰值千万级并发长连接 · OpenAI TLM / Inference Infra  
> **核心考点：** 高并发长连接、传输协议选型、RPS vs TPS、TTFT、Barge-in  
> **原则：** Model-Aware · Ambiguity · Deep Dive Into Internals

---

## 0. 澄清需求（先问再画）

| 维度 | 假设（面试自报） | 设计影响 |
|------|------------------|----------|
| DAU / 峰值并发 | 1 亿注册；峰值 **500 万** 同时流式会话 | 连接层 + GPU 池容量 |
| 平均输出长度 | ~200 tokens / reply | 总 TPS ≈ 并发 × 每会话 decode rate |
| SLA | TTFT P50 < 300ms，P99 < 800ms；TPOT ~30–50ms | Prefill 优先队列、路由策略 |
| 客户端 | Web 主路径 + 移动端 + 嵌入式语音 | SSE vs WebRTC 分路径 |
| 打断 | 用户可随时 Stop / Barge-in | cancel 协议 + KV 回滚 |

**开场金句：**

> 这不是「高并发 HTTP」题——瓶颈在 **GPU 上的 Token 吞吐与 KV Cache 显存**。我会分开设计：连接平面（长连接 / 流式推送）与推理平面（Prefill/Decode），并用 cancel 把两边耦在一起。

---

## 1. 指标：RPS vs TPS（必先对齐）

| 指标 | 定义 | 主要消耗资源 | 扩容手段 |
|------|------|--------------|----------|
| **RPS** | 每秒新建/活跃的 chat **请求**（连接） | 网关、连接表、Auth、调度器 | 水平扩 Edge / Gateway |
| **TPS** | 每秒生成/下发的 **Token** 数 | GPU 算力、HBM 带宽、KV 显存 | 加卡、continuous batching、量化、小模型路由 |

**粗算（面试白板）：**

```
峰值并发会话 C = 5e6
平均 decode 速率 r ≈ 30 token/s（单会话）
瞬时总需求 TPS ≈ C × r = 1.5e8 token/s  （不可全量同时全速——真实有排队）

有效 GPU TPS 容量由 continuous batching 决定：
  单卡 effective ≈ 数百～数千 token/s（视模型与 batch）
  需要的卡数 N ≈ 目标 TPS / 单卡 TPS × 冗余(1.3–1.5)
```

> **金句：** RPS 决定「有多少人在问」；TPS 决定「GPU 每秒能吐多少字」。ChatGPT 在高峰几乎总是 **TPS / 显存 bound**，不是网关 RPS bound。

---

## 2. 传输协议选型（Deep Dive）

### 2.1 对比表

| 协议 | 方向 | 延迟 | 移动端耗电 / 连接 | 中间件兼容 | 结论 |
|------|------|------|-------------------|------------|------|
| HTTP 短轮询 | C→S 反复拉 | 高（轮询间隔） | 差（频繁唤醒） | 最好 | ❌ 不适合流式 |
| WebSocket | 全双工 | 低 | 保活心跳耗电；LB sticky/超时复杂 | 中 | 工具调用、双向控制可用 |
| **SSE** | S→C 单向 | 低（首 byte 即推） | 走 HTTP/2；断线浏览器可自动重连 | 好 | ✅ **文本 Chat 主路径** |
| **WebRTC** | 媒体 + DataChannel | 最低（UDP+FEC） | 适音频；信令复杂 | 需 TURN | ✅ **语音 / 嵌入式** |

### 2.2 为什么 ChatGPT 文本路径选 SSE，而不是 WebSocket？

1. **业务是「请求 → 单向 Token 流」**：客户端上行只是 prompt + 少量 cancel；不需要常开全双工。
2. **HTTP/2 / CDN / 企业代理** 对 SSE 更友好；WebSocket 常被防火墙掐掉。
3. **移动端：** WebSocket 为保活需心跳，后台易被系统杀；SSE 可按请求生命周期存在，Stop 即关连接。
4. **运维：** SSE 就是长 HTTP 响应，日志、鉴权、速率限制复用现有 API Gateway。

**WebSocket 仍用在：** 工具调用双向交互、协作编辑、或需要 server 主动 push 多类事件时。

### 2.3 为什么语音 / 嵌入式选 WebRTC？

- TCP（含 SSE over TCP）丢包 → 重传 → **队头阻塞**，音频卡顿。
- WebRTC：UDP + FEC + jitter buffer，优先实时性；Barge-in 控制可走 DataChannel。

### 2.4 SSE 落地要点（防缓冲杀死 TTFT）

```http
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
X-Accel-Buffering: no

data: {"type":"token","index":0,"text":"Hello"}

data: {"type":"token","index":1,"text":" world"}

data: {"type":"done","usage":{"completion_tokens":2}}
```

| 陷阱 | 处理 |
|------|------|
| Nginx / CDN 缓冲整段响应 | `X-Accel-Buffering: no`；proxy_buffering off；chunked |
| 应用层 `print` 不 flush | 每 token `flush()`；async 写 socket |
| 空闲超时 | 周期性 `: keepalive\n\n` 注释帧 |
| 客户端断连 | 检测 write error → 发 **cancel** 到 GPU worker（见 §5） |

---

## 3. 高层架构（1 亿用户规模）

```
                         ┌─────────────────────────────────────────┐
  Clients                │              Edge / CDN                 │
  (Web SSE /             │  TLS · WAF · Geo · Anycast              │
   Mobile /              └──────────────────┬──────────────────────┘
   Embedded)                                │
                         ┌──────────────────▼──────────────────────┐
                         │           API Gateway                   │
                         │  Auth · RPM/TPM · request validation    │
                         └──────────────────┬──────────────────────┘
                                            │
              ┌─────────────────────────────┼─────────────────────────────┐
              │                             │                             │
              ▼                             ▼                             ▼
     ┌────────────────┐          ┌──────────────────┐          ┌─────────────────┐
     │ Session Store  │          │ Stream Gateway   │          │ Control Plane   │
     │ (conversation, │◄─────────│ (SSE fan-out,    │◄─────────│ cancel / barge  │
     │  sticky map)   │          │  generation_id)  │          │ rate limit      │
     └────────┬───────┘          └────────┬─────────┘          └────────┬────────┘
              │                           │                             │
              │              ┌────────────▼────────────┐                │
              │              │   Inference Scheduler   │◄───────────────┘
              │              │   (queue · priority ·   │
              │              │    prefix-cache aware)  │
              │              └────────────┬────────────┘
              │                           │
              │         ┌─────────────────┼─────────────────┐
              │         ▼                 ▼                 ▼
              │   ┌──────────┐      ┌──────────┐      ┌──────────┐
              └──►│ GPU Pool │      │ GPU Pool │      │ GPU Pool │  AZ-a/b/c
                  │ Prefill  │      │ Decode   │      │ SpecDec  │
                  │ + KV     │      │ Continuous│     │ Workers  │
                  └──────────┘      │ Batching  │      └──────────┘
                                    └──────────┘
```

### 3.1 两平面分离

| 平面 | 职责 | 扩容 |
|------|------|------|
| **连接平面** Stream Gateway | 维持百万～千万 SSE；token 下发；断连检测 | 无状态水平扩；连接数 / 机 |
| **推理平面** Scheduler + GPU | Prefill / Decode / KV | 按 **TPS + KV 显存** 扩卡 |

Stream Gateway **绝不**在进程内跑模型——否则连接风暴会拖垮推理。

### 3.2 Sticky 路由

- `conversation_id → worker_id`（前缀 KV / 会话缓存命中）
- 会话续聊优先打到 **持有该 KV 的 GPU**；miss 则冷启动 Prefill
- Worker 宕机：会话映射失效 → 下一请求全量 Prefill（可接受）

---

## 4. 流式路径：压低 TTFT

### 4.1 端到端时序

```
Client                Gateway           Scheduler          GPU Worker
  │  POST /chat/stream  │                  │                  │
  │────────────────────►│                  │                  │
  │  200 + SSE headers  │                  │                  │
  │◄────────────────────│                  │                  │
  │                     │  enqueue job     │                  │
  │                     │─────────────────►│  schedule        │
  │                     │                  │─────────────────►│ Prefill
  │                     │                  │                  │──► first token
  │  data: token[0]     │◄──── push ───────│◄──── stream ─────│  (flush!)
  │◄────────────────────│                  │                  │ Decode loop
  │  data: token[1..]   │◄─────────────────│◄─────────────────│
  │◄────────────────────│                  │                  │
  │  data: [DONE]       │                  │                  │
  │◄────────────────────│                  │                  │
```

### 4.2 TTFT 优化清单

| 手段 | 机制 | 收益 |
|------|------|------|
| **首 Token 即 Flush** | Worker → Gateway → Client 全路径禁缓冲 | 去掉 100ms+ 假延迟 |
| **Warm sticky** | 同会话打到已有 KV 的 worker | 跳过重复 Prefill |
| **Prefix cache** | 系统 prompt / RAG 前缀 hash 命中 | Prefill 变增量 |
| **SLO 分层队列** | 短 prompt / VIP 高优先；长 Prefill 隔离 | 保护 P99 TTFT |
| **Speculative Decoding** | Draft 小模型 + 大模型验证 | 降 TPOT，间接改善体感 |
| **模型路由** | 简单 query → 小模型 | Prefill 更短 |

### 4.3 Prefill vs Decode（Model-Aware）

| 阶段 | Bound | 调度 |
|------|-------|------|
| Prefill | **Compute-bound**（大矩阵） | 可 batch 多个短 prompt；避免超长 prompt 饿死短请求 |
| Decode | **Memory-bound**（反复读 KV） | Continuous batching；PagedAttention 管显存 |

详见仓库：[../LLM/kv_cache.py](../LLM/kv_cache.py) · [../LLM/paged_attention.py](../LLM/paged_attention.py)

---

## 5. Barge-in / 异步打断（Cancel）

### 5.1 协议

```
generation_id:  uuid          # 每次回复唯一
commit_cursor:  int           # 客户端已展示/确认的最后一个 token index
```

**客户端 Stop / 语音打断：**

```
POST /v1/chat/cancel
{ "generation_id": "...", "commit_cursor": 42 }
```

或在 WebRTC DataChannel 上发同一 JSON（语音路径）。

### 5.2 服务端处理

```
1. Control Plane 查 generation_id → worker_id
2. 向 Worker 发 abort（进程内 cancellation token / IPC）
3. Worker:
   - 停止 Decode 循环（不再占 GPU batch slot）
   - KV Cache 截断到 commit_cursor（或整段丢弃本次 assistant turn）
   - 释放 PagedAttention blocks 回空闲池
4. Stream Gateway 向客户端发 `event: cancelled` 并关闭 SSE
```

### 5.3 竞态

| 竞态 | 处理 |
|------|------|
| cancel 到达时 token 已在途 | 客户端以 `commit_cursor` 为准丢弃后续 SSE |
| 双击 Stop | cancel 幂等；generation_id 结束后忽略 |
| 断连未发 cancel | Gateway 写失败 → 自动 cancel（防 GPU 空转） |

### 5.4 为什么必须做 Cancel？

在 500 万并发下，若用户关掉页面而 GPU 继续生成 200 token：

```
浪费 ≈ 空转会话数 × 剩余 tokens × 单 token 成本
```

**断连即 cancel** 是成本与容量的硬需求，不是体验优化。

---

## 6. 连接层：1 亿用户级长连接

### 6.1 容量粗算

```
峰值 SSE 连接 C = 5e6
单 Stream Gateway 进程：~5e4–2e5 连接（epoll + 异步 IO，视机型）
Gateway 实例数 ≈ C / 连接数每机 × 冗余
```

- 连接状态尽量 **无状态**：`generation_id` 元数据放 Redis / 会话服务
- **心跳：** SSE comment 每 15s，防止 NAT / LB 掐空闲连接
- **分区：** 按 `user_id` 或 `conversation_id` 一致性哈希到 Gateway 集群，便于 cancel 路由

### 6.2 背压

当 Scheduler 队列深度 > 阈值：

1. Gateway **拒绝新 SSE**（503 + Retry-After）——保护已有会话
2. 降低新会话 `max_tokens`
3. 路由部分流量到小模型（Graceful Degradation）

---

## 7. 10x / 100x Scaling Drill（主动讲）

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1×（~5e4 并发） | 单区 GPU 排队 | 扩卡；开 continuous batching |
| 10× | **KV 显存** + Scheduler 热点 | PagedAttention；分片前缀缓存；多 AZ |
| 100× | Decode 带宽不可压缩 | 强制摘要压缩长会话；非 VIP 小模型；队列准入；区域分流 |

> 「GPU 推理延迟不能靠加网关解决。100x 时我优先做 **准入控制 + 模型降级 + KV 显存治理**，而不是盲目加 Web 机器。」

---

## 8. 观测与 SLO

| 指标 | 用途 |
|------|------|
| TTFT P50/P99 | Prefill / 调度健康 |
| TPOT / TPS | Decode 与卡利用率 |
| Queue wait | 是否该扩卡或拒流 |
| Cancel rate | 用户打断 / 断连比例（成本） |
| SSE write error | 连接质量；自动 cancel 触发率 |
| KV cache hit rate | sticky / prefix cache 是否生效 |

---

## 9. 面试口述 5 分钟版

1. **澄清：** 并发、TTFT、客户端类型；强调 RPS ≠ TPS。  
2. **协议：** 文本 SSE（单向、HTTP 友好、省电）；语音 WebRTC；否掉短轮询。  
3. **架构：** Edge → Gateway(RPM/TPM) → Stream Gateway ∥ Scheduler → GPU（Prefill/Decode）。  
4. **TTFT：** 禁缓冲 flush、sticky KV、prefix cache、优先级队列。  
5. **Barge-in：** `generation_id` + cancel → 停 Decode、释 KV；断连自动 cancel。  
6. **Scale：** 100x 先崩显存/TPS → 降级与准入，而非加无状态 Web 节点。

---

## 10. 参考实现

| 文件 | 内容 |
|------|------|
| [streaming_chat_demo.py](./streaming_chat_demo.py) | SSE 风格 token 推送 + cancel / barge-in |
| [token_rate_limiter.py](./token_rate_limiter.py) | 网关侧 RPM + TPM |
| [../LLM/kv_cache.py](../LLM/kv_cache.py) | Decode 阶段 KV Cache |
| [../LLM/paged_attention.py](../LLM/paged_attention.py) | 多租户显存块管理 |

```bash
cd openai
python3 streaming_chat_demo.py
```

---

## 11. 追问清单（自测）

- [ ] 为什么不用短轮询？WebSocket 在什么场景反而更好？  
- [ ] TTFT 和 TPOT 分别对应 Prefill 还是 Decode？  
- [ ] Nginx 默认缓冲如何毁掉流式体验？  
- [ ] 用户关页面但不点 Stop，GPU 会怎样？如何自动回收？  
- [ ] sticky 会话 worker 挂了怎么办？  
- [ ] 峰值 TPS 怎么从并发和 decode rate 估算？  
- [ ] 100x 流量时你先动哪三个旋钮？
