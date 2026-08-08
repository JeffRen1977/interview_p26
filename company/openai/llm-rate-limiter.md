# 设计多租户 LLM 智能体配额与速率限制器（RPM + TPM）

> **核心考点：** 传统限流只计请求次数；LLM 的计费与 GPU 负载按 **Token** 计  
> **必须覆盖：** 双维度令牌桶（RPM + TPM）、动态权重、分布式同步（Redis / Local Cache + 异步 flush）  
> **关联 Demo：** [token_rate_limiter.py](./token_rate_limiter.py)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 必问 / 假设 | 设计影响 |
|------|-------------|----------|
| 租户模型 | Org / Project / API Key 多层配额？ | 限流 key 层级与继承 |
| 限额维度 | **RPM** + **TPM**（input/output 是否分开）？ | 双桶或三桶 |
| 一致性 | 严格全局不超限 vs 允许短暂 5–10% 超卖？ | Redis 强一致 vs 本地近似 |
| 延迟预算 | 限流本身 P99 < 1–2ms？ | 本地缓存 + 异步 flush |
| 失败模式 | Redis 挂了：fail-open 还是 fail-closed？ | 安全 vs 可用性 |

**开场金句：**

> 这不是「Redis INCR 防刷」题。LLM 一次请求可能是 10 token，也可能是 128K context——只限 RPM 会让长请求打穿 GPU。我要设计 **RPM + TPM 双令牌桶**，并在分布式网关上用 **本地配额切片 + 异步对账**，避免限流器自己变成瓶颈。

---

## 1. 为什么传统限流不够

| 方案 | 计量单位 | 对 LLM 的问题 |
|------|----------|----------------|
| 固定窗口计数 | 请求数 | 忽略 Token 体积；窗口边界突刺 |
| 滑动窗口 / 简单令牌桶 | 请求数 | 同上；单次超大 prompt 仍放行 |
| 仅 TPM | Token | 无法防「极短请求打爆连接/调度」 |
| **RPM + TPM 双桶** | 请求 + Token | ✅ 同时保护 **调度面** 与 **算力面** |

**Model-Aware：**

- **RPM** ≈ 保护 Gateway / Scheduler 队列（接近 RPS）
- **TPM** ≈ 保护 GPU Prefill+Decode 负载与账单

---

## 2. 双维度令牌桶算法

### 2.1 每个租户两套桶

```
rpm_bucket:
  capacity     = RPM_limit          # 桶深 = 每分钟允许突发
  refill_rate  = RPM_limit / 60     # 每秒回补

tpm_bucket:
  capacity     = TPM_limit
  refill_rate  = TPM_limit / 60
```

**准入（原子语义）：**

```
estimate = prompt_tokens + max(estimated_completion, min_reserve)
IF !rpm.try_consume(1):           DENY 429 rpm_exceeded
IF !tpm.try_consume(estimate):    refund rpm; DENY 429 tpm_exceeded
ALLOW  → 进入推理；结束后 reconcile(actual_tokens)
```

> TPM 桶消费的是 **权重 = Token 数**，不是固定 1——这就是「动态权重令牌桶」。

### 2.2 动态权重：如何估 Token？

| 阶段 | 权重来源 | 说明 |
|------|----------|------|
| **准入前** | tokenizer 计数 prompt + `max_tokens`（或历史 P95 completion） | 保守：宁可多扣，完成后退还 |
| **流式中** | 可选：每 N 个 output token 增量扣 TPM | 长生成中途熔断 |
| **结束后** | `actual = prompt + completion` | `refund = estimate - actual`（若 estimate > actual） |

```
estimate = 800, actual = 520  →  refund 280 到 tpm_bucket
estimate = 800, actual = 950  →  额外 try_consume(150)；失败则记债/降级下轮
```

**智能体（Agent）场景：** 多步 Tool Call 每步单独 `allow()`，或给 agent run 设 **TPM 子预算**（parent bucket 派生）。

### 2.3 与 Leaky Bucket / 固定窗口对比

| 算法 | 突发 | 实现复杂度 | LLM 适配 |
|------|------|------------|----------|
| 固定窗口 | 窗口边界双倍突发 | 低 | 差 |
| 滑动日志 | 平滑 | 内存高 | 中 |
| **Token Bucket** | 允许受控突发（桶深） | 低 | ✅ 权重可变 |
| Leaky Bucket | 恒定流出 | 中 | 适合整形，不适合突发短请求 |

面试选 **Token Bucket**：API 产品需要短时突发（用户连点），又要长期速率受控。

### 2.4 HTTP 响应约定

```http
HTTP/1.1 429 Too Many Requests
Retry-After: 12
x-ratelimit-limit-requests: 500
x-ratelimit-remaining-requests: 0
x-ratelimit-limit-tokens: 90000
x-ratelimit-remaining-tokens: 1200
x-ratelimit-reset-tokens: 12
```

---

## 3. 系统架构

```
Client
  │
  ▼
API Gateway (多实例)
  │  1) Auth → tenant_id
  │  2) RateLimiter.allow(tenant, estimate)
  │  3) 429 or forward
  ▼
Inference Scheduler → GPU Workers
  │
  ▼
完成后 Usage Reporter → RateLimiter.reconcile(tenant, actual)
```

```
                    ┌──────────────────────────────────────┐
                    │         Config / Quota Service        │
                    │  tenant → {rpm, tpm, tier, burst}     │
                    └──────────────────┬───────────────────┘
                                       │ pull / push
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
  Gateway-1                     Gateway-2                     Gateway-N
  ┌─────────────────┐           ┌─────────────────┐
  │ LocalShardCache │           │ LocalShardCache │  ...
  │  rpm/tpm slices │           │                 │
  │  async flusher  │──────────►│                 │
  └────────┬────────┘           └────────┬────────┘
           │                             │
           └──────────┬──────────────────┘
                      ▼
              Redis Cluster
              key: rl:{tenant}:rpm | rl:{tenant}:tpm
              (Lua: refill + consume 原子)
```

---

## 4. 分布式高并发同步（核心 Deep Dive）

限流器自己的延迟若到 5–10ms，会直接抬高 TTFT。目标：**热路径本地决策，后台对账**。

### 4.1 方案对比

| 方案 | 延迟 | 全局准确性 | 热点 | 适用 |
|------|------|------------|------|------|
| A. 每次 Redis Lua | 1–3ms+ | 强一致 | 热 tenant 打爆 slot | 中低 QPS / 严合规 |
| B. Redis Cluster 分片 | 中 | 强 | 大租户仍单 key 热 | 中等规模 |
| **C. Local Cache + 异步 Flush** | **<0.1ms** | 近似（可配安全余量） | 分散 | **高 QPS 网关** |
| D. 租户一致性哈希到固定 Gateway | 低 | 该分片内准 | 需会话粘滞 | 可与 C 组合 |

### 4.2 推荐：配额切片（Quota Slicing）+ 异步 Flush

**思想：** 把租户的全局 TPM/RPM **预分片**到各 Gateway 本地桶，本地先扣；周期性把用量 flush 到 Redis，再拉取新切片。

```
全局 TPM = 90_000 / min
Gateway 数 G = 30
每机本地切片 ≈ (TPM / G) × safety_factor(0.8~0.9)

例：每机 ~2400 TPM/min 本地容量
本地耗尽 → 同步向 Redis 再申请一块 / 或直接 429
```

**为什么乘 safety_factor < 1？**  
异步 flush 窗口内多机会超卖；预留 10–20% 全局余量换延迟。

**Flush 循环（每 50–100ms）：**

```
1. 汇总本窗口 local_consumed_rpm / local_consumed_tpm
2. Redis Lua: global_consume(tenant, delta_rpm, delta_tpm)
3. 若全局拒绝：标记 tenant 进入 cooldown；本地桶置 0
4. 若成功：按剩余全局配额重新分配 next_slice 到本地
```

### 4.3 Redis Cluster + Lua（强一致路径）

用于：**大客户严格配额**、计费审计、或本地切片耗尽后的同步申请。

伪代码（单 key 原子 refill+consume）：

```lua
-- KEYS[1]=bucket_key  ARGV: capacity, refill_per_ms, now_ms, cost
local data = redis.call('HMGET', KEYS[1], 'tokens', 'ts')
local tokens = tonumber(data[1]) or tonumber(ARGV[1])
local ts = tonumber(data[2]) or tonumber(ARGV[3])
local elapsed = math.max(0, tonumber(ARGV[3]) - ts)
tokens = math.min(tonumber(ARGV[1]), tokens + elapsed * tonumber(ARGV[2]))
local cost = tonumber(ARGV[4])
if tokens < cost then
  redis.call('HMSET', KEYS[1], 'tokens', tokens, 'ts', ARGV[3])
  return {0, tokens}  -- deny
end
tokens = tokens - cost
redis.call('HMSET', KEYS[1], 'tokens', tokens, 'ts', ARGV[3])
return {1, tokens}    -- allow
```

**Cluster 注意：**

- `rpm` 与 `tpm` 两个 key 用 **hash tag** `{tenant_id}` 保证同 slot，便于 pipeline
- 热租户：考虑 **cell-based** 限流（租户拆成 N 个虚拟子桶，随机选一个扣）降低单 key 压力

### 4.4 本地缓存结构（Gateway 内）

```
TenantLocalState:
  rpm_tokens, tpm_tokens
  last_refill_ts
  pending_flush_rpm, pending_flush_tpm   # 待上报
  cooldown_until                        # 全局拒绝后短暂熔断
```

- 热路径：`mutex` 或 per-tenant `atomic` + 无锁 refill 公式  
- **绝不**在请求路径同步等 Redis（除非本地切片用尽）

### 4.5 失败模式

| 故障 | 策略 | 话术 |
|------|------|------|
| Redis 超时 | 短时 fail-open（带本地硬顶）或 fail-closed（付费 API） | 「计费租户 fail-closed；免费层可 fail-open + 降级模型」 |
| Flush 积压 | 加大 batch；本地桶见顶先拒 | 保护 GPU 优先于精确账单 |
| 时钟漂移 | Redis 侧用服务器时间做 refill | 多机 `monotonic` 只用于本地 |

---

## 5. 多租户与智能体细节

### 5.1 配额层级

```
Org TPM ─┬─ Project A TPM
         ├─ Project B TPM
         └─ API Key burst 乘数
```

子级 `min(自身限额, 父级剩余)`；父级用 **共享桶** 或 **预留分配**。

### 5.2 Agent / Tool Calling

一次用户消息可能触发多轮模型调用：

| 策略 | 做法 |
|------|------|
| 逐步扣减 | 每轮 LLM/tool 前 `allow(estimate)` |
| Run 预算 | 创建 `run_id` 子桶：`tpm_budget = min(剩余, agent_cap)` |
| 工具权重 | 代码解释器等按 **折合 token** 或独立 RPM |

### 5.3 动态权重进阶

- **模型倍率：** `gpt-4o` 权重 1.0，`o1` 权重 3.0（算力更贵）→ `cost = tokens * model_weight`
- **缓存命中：** prefix / semantic cache 命中 → TPM 只扣极小常量（仍扣 RPM）
- **流式中途超限：** 发 `rate_limit` 事件并 cancel generation（与流式后端 cancel 协议打通）

---

## 6. 10x / 100x Scaling Drill

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1× | 单 Redis key | hash tag + 分片；本地切片 |
| 10× | 热租户 Lua QPS | cell 子桶；Gateway 粘滞；提高 flush 批量 |
| 100× | 限流变 TTFT 瓶颈 | **默认本地决策**；Redis 仅对账；严格租户单独集群 |

> 「100x 时我不会让每个请求都打 Redis。限流热路径必须在 Gateway 内存完成；Redis 是配额权威源，不是每请求锁。」

---

## 7. 观测指标

| 指标 | 含义 |
|------|------|
| `ratelimit_denied_rpm / tpm` | 哪一维在挡流量 |
| `ratelimit_local_latency_us` | 热路径是否健康 |
| `ratelimit_redis_flush_errors` | 对账是否落后 |
| `ratelimit_oversell_ratio` | 近似模式超卖幅度 |
| `estimate_vs_actual_err` | 动态权重估测偏差 |

---

## 8. 面试口述 3 分钟版

1. **问题：** 只限 RPM 挡不住长 context；要 RPM + TPM。  
2. **算法：** 双 Token Bucket；TPM 权重 = 预估 Token；结束后 reconcile。  
3. **分布式：** 热路径 Local shard + 异步 flush；权威状态在 Redis Lua；热租户 cell 分片。  
4. **Agent：** 多步逐步扣或 run 级子预算；模型倍率动态权重。  
5. **Scale：** 100x 时限流不能同步依赖 Redis，否则自己先于 GPU 成为瓶颈。

---

## 9. 参考实现

```bash
cd openai
python3 token_rate_limiter.py
```

覆盖：

- 双桶 RPM/TPM  
- 动态权重 estimate + reconcile refund  
- `LocalShardLimiter`：本地切片 + 异步 flush 到进程内 mock Redis  

---

## 10. 追问清单（自测）

- [ ] estimate 大于 actual 如何退还？小于呢？  
- [ ] 为什么双桶都要扣，而不是只扣 TPM？  
- [ ] Local slice 的 safety_factor 防止什么？  
- [ ] Redis Cluster 上 rpm/tpm 两个 key 如何同 slot？  
- [ ] Redis 宕机时付费 API 选 fail-open 还是 fail-closed？  
- [ ] Agent 10 步工具调用如何避免第一步耗尽导致后续全死？
