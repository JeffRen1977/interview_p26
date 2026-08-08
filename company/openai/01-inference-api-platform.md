# 01 - 大模型基础设施与 API 平台（Inference & API Platform）

> **岗位：** OpenAI TLM / Inference Infra · **原则：** Model-Aware + Deep Dive

---

## Q1. 设计支持实时流式传输的 ChatGPT 后端系统

> **完整标准答案（架构图、协议 Deep Dive、TTFT、Barge-in、1 亿用户扩容）：**  
> [streaming-chatgpt-backend.md](./streaming-chatgpt-backend.md) · Demo：[streaming_chat_demo.py](./streaming_chat_demo.py)

### 1.1 澄清需求（Ambiguity）

| 维度 | 必问 |
|------|------|
| 规模 | DAU、峰值并发长连接数、单会话平均 Token 长度 |
| SLA | **TTFT** P50/P99、每 Token 延迟、端到端超时 |
| 模型 | 单模型 vs 多模型路由；是否 Speculative Decoding |
| 客户端 | Web / iOS / Android / 嵌入式 — 连接与耗电约束不同 |

### 1.2 核心指标：RPS vs TPS

| 指标 | 定义 | 设计影响 |
|------|------|----------|
| **RPS** | 每秒 HTTP/SSE 连接或请求数 | 网关、Auth、连接表容量 |
| **TPS** | 每秒生成/传输 **Token** 数 | GPU 算力、HBM 带宽、KV Cache 显存 |

> 面试金句：ChatGPT 的瓶颈通常不是 RPS，而是 **GPU 上的 TPS 与 KV Cache 显存**；Prefill 吃算力，Decode 吃带宽。

### 1.3 高层架构

```
Client ──► Edge CDN / API Gateway (Auth, Rate Limit)
              │
              ▼
         Session Router (sticky by conversation_id)
              │
              ▼
         Inference Scheduler ──► GPU Worker Pool (vLLM / TGI)
              │                      │
              │                      ├─ Prefill batch
              │                      └─ Decode continuous batching
              ▼
         Stream Multiplexer ──► SSE / WebRTC data channel
```

### 1.4 传输协议选型（必 Deep Dive）

| 协议 | 优点 | 缺点 | OpenAI 场景 |
|------|------|------|-------------|
| **HTTP 短轮询** | 简单 | 高延迟、浪费连接 | ❌ 不适合流式 |
| **WebSocket** | 全双工 | 移动端保活耗电、中间件超时复杂 | 工具调用双向场景可用 |
| **SSE (Server-Sent Events)** | 单向流式、HTTP/2 友好、自动重连 | 仅 server→client | ✅ ChatGPT Web 主流 |
| **WebRTC** | UDP、低延迟、FEC | 信令复杂 | ✅ 语音/嵌入式实时 |

**SSE 实现要点：**

```http
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive

data: {"token": "Hello", "index": 0}

data: {"token": " world", "index": 1}

data: [DONE]
```

- 第一个 Token 产生后 **立即 flush**（禁用 Nginx/代理缓冲：`X-Accel-Buffering: no`）
- 客户端断连：Inference Worker 收到 **cancel token** 停止 KV 继续生成（省 GPU）

### 1.5 TTFT 优化

1. **路由到 warm GPU** — 会话 sticky，复用已有 KV Cache 所在 worker
2. **Prompt 前缀缓存** — 系统 Prompt / 文档前缀 hash 命中则跳过 Prefill
3. **小 Prompt 优先队列** — 避免长 Prefill 阻塞短请求（SLO 分层）
4. **Speculative Decoding** — 小 draft 模型 + 大模型验证（降 per-token 延迟）

### 1.6 Barge-in（异步打断）

```
User speaks ──► Edge VAD detects speech start
                    │
                    ├─► Client: stop audio playback
                    ├─► Client: POST /cancel {stream_id}
                    └─► Server: abort generation, truncate KV cache to last committed token
```

- 上下文窗口 **精准回滚**：只保留已确认（played/ack）的 assistant tokens
- 需 **generation_id + commit_cursor** 协议，避免 race

### 1.7 10x / 100x Scaling Drill

> 「1 万并发时 Scheduler + GPU pool 够用；100 万并发时 **最先崩的是 GPU KV Cache 显存与 Decode 带宽**，因为推理延迟不可压缩。我会引入：动态 batch 上限、长上下文降级（摘要压缩）、模型路由到小模型、队列优先级 + Graceful Degradation（返回『系统繁忙，请缩短输入』）。」

---

## Q2. 多租户 LLM 配额与速率限制器（RPM + TPM）

> **完整标准答案（动态权重双桶、Redis Lua、Local Cache + 异步 Flush）：**  
> [llm-rate-limiter.md](./llm-rate-limiter.md) · Demo：[token_rate_limiter.py](./token_rate_limiter.py)

### 2.1 为什么传统 Redis 计数器不够

- SaaS 按 **Token 计费**，仅限制 RPM 无法防止「单次 128K context 打穿 GPU」
- 需 **双维度**：Requests Per Minute (RPM) + Tokens Per Minute (TPM)

### 2.2 双维度令牌桶

每个 tenant 维护：

```
rpm_bucket:  capacity = RPM_limit,  refill = RPM_limit / 60 per second
tpm_bucket:  capacity = TPM_limit,  refill = TPM_limit / 60 per second
```

**请求准入：**

1. 预估本次 `prompt_tokens + max_new_tokens`（或滑动平均）
2. 同时 `try_consume(rpm, 1)` 和 `try_consume(tpm, estimated_tokens)`
3. 任一失败 → `429 Too Many Requests` + `Retry-After`

### 2.3 分布式一致性 vs 延迟

| 方案 | 延迟 | 一致性 | 适用 |
|------|------|--------|------|
| 每次 Redis INCR | 高 | 强 | 低 QPS |
| **本地计数 + 异步刷 Redis** | 低 | 近似 | 高 QPS 网关 |
| Redis Cluster + Lua 原子脚本 | 中 | 强 | 中等规模 |

**本地缓存 + 异步刷盘：**

```
Gateway 本地 atomic counter ──► 每 100ms 批量 flush 到 Redis
                              └── 超限本地先拒，防止 Redis 热点
```

### 2.4 参考实现

见 [token_rate_limiter.py](./token_rate_limiter.py)

---

## Q3. 基于语义缓存（Semantic Cache）的 RAG 系统

> **完整标准答案（Vector DB cosine、θ=0.95、TTL、定点/邻域/纪元失效）：**  
> [semantic-cache-rag.md](./semantic-cache-rag.md) · Demo：[semantic_cache.py](./semantic_cache.py)

### 3.1 目标

相似问题不调用大模型 → **降成本、降延迟**。

### 3.2 流程

```
User Query
    │
    ▼
Embedding Model ──► query_vector
    │
    ▼
Vector DB ANN Search (top-k)
    │
    ├─ cosine_sim >= 0.95 ──► return cached answer (+ metadata)
    └─ else ──► RAG retrieve ──► LLM generate ──► write cache
```

### 3.3 命中策略与失效

| 策略 | 说明 |
|------|------|
| **阈值** | cosine ≥ 0.95（按业务 A/B 调） |
| **TTL** | 缓存 entry 24h–7d 过期 |
| **知识库更新** | 文档 chunk 变更 → 标记 `kb_version`；查询时 `cache.kb_version == current` |
| **批量失效** | 更新文档 embedding 后，ANN 找相近 query vectors 批量 delete |

### 3.4 陷阱（Deep Dive）

- **语义碰撞**：不同意图但 embedding 相近 → 加 intent classifier 或 LLM 二次确认
- **个性化**：同一问题不同用户权限不同 → cache key = `(tenant, user_role, query_emb_bucket)`
- **Stale answer**：TTL + kb_version 双保险

### 3.5 参考实现

见 [semantic_cache.py](./semantic_cache.py)

---

## 关联代码

- [LLM/kv_cache.py](../LLM/kv_cache.py) — KV Cache / PagedAttention
- [LLM/paged_attention.py](../LLM/paged_attention.py) — vLLM 风格显存管理
