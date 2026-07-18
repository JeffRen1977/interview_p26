# 04 - OpenAI TLM 面试高分通关秘籍

---

## 1. 与传统 System Design 的区别

| 传统大厂 | OpenAI |
|----------|--------|
| CRUD + 缓存 + 分库分表 | **GPU / Token / 模型行为** 驱动架构 |
| QPS 均匀 | **Prefill burst + Decode 长尾** |
| 组件堆叠 | **每个组件的失效模式** |
| 需求明确 | **主动澄清 Ambiguity** |

---

## 2. 答题框架（45 min）

```
0–5 min   Clarify + SLA + 规模假设（写出数字）
5–15 min  High-level diagram（Model-Aware 数据流）
15–30 min Deep dive 2–3 组件（选面试官最感兴趣的）
30–40 min Failure modes + monitoring
40–45 min 10x / 100x Scaling Drill（主动）
```

### 开场模板

> 「我先确认三个约束：峰值并发、TTFT/TPOT SLA、以及成本预算。假设 1000 万 DAU、峰值 50 万并发会话、P99 TTFT < 800ms。我会区分 Prefill 与 Decode，因为硬件 bound 不同……」

---

## 3. 规则一：No Buzzwords Drop

提到任何组件，必须准备 **原理 + 失效模式** 追问。

| 你说了 | 面试官会追问 |
|--------|--------------|
| **Kafka** | Partition 分配策略？Consumer rebalance 停几秒时语音流怎么办？Exactly-once 是否必要？ |
| **Redis** | 限流热 key？Cluster slot 迁移？Lua 脚本原子性？ |
| **vLLM** | Continuous batching 如何 preemption？PagedAttention block 大小？ |
| **WebRTC** | ICE 失败 fallback？TURN 成本？SFU vs MCU？ |
| **Firecracker** | Snapshot 大小？启动路径 vs QEMU？ |

**正确姿势：** 「我们用 Kafka 做 **async audit log**，不是实时路径 — 实时走 UDP。Kafka 分区按 `tenant_id` 保序；rebalance 用 cooperative-sticky protocol 减少 stop-the-world。」

---

## 4. 规则二：The Scaling Drill（10x / 100x）

每完成初版设计，**主动**说：

> 「这个架构在 **1 万** 用户时 OK。到 **100 万** 时，最先崩的是 **GPU KV Cache 显存**，因为 Decode 延迟不可压缩。我会：
> 1. 引入 continuous batching 上限与 max context 硬截断
> 2. 长会话自动摘要压缩 history
> 3. 非 VIP 租户路由到更小模型（Graceful Degradation）
> 4. 队列超深时返回 503 + Retry-After，保护 GPU 不被 OOM」

### 常见「最先崩」组件清单

| 系统 | 1x 瓶颈 | 100x 先崩 |
|------|---------|-----------|
| 流式 Chat | 单区 GPU | KV 显存 + Scheduler 队列 |
| 语音助手 | SFU CPU | TTS/LLM GPU + UDP 拥塞 |
| 代码沙箱 | Warm pool | Orchestrator + snapshot IO |
| 边缘相机 | 端侧 NPU | 云端 triggered inference 成本 |

---

## 5. Model-Aware 必背概念

| 概念 | 一句话 |
|------|--------|
| TTFT | 首 Token 延迟 — Prefill 主导 |
| TPOT | 每 Token 时间 — Decode 主导 |
| KV Cache | 避免 O(N²) 重复计算；PagedAttention 防碎片 |
| Continuous Batching | 动态插入/退出请求，提高 GPU 利用率 |
| RPM / TPM | 双维度限流 |
| Semantic Cache | Embedding 相似命中，省模型调用 |

---

## 6. Embedded Experiences 加分项

- **dmabuf 零拷贝**、NPU pipeline
- **Barge-in** 协议与 KV 回滚
- **端云 cascade** + confidence threshold
- **弱网 FEC** vs TCP 重传权衡
- **功耗**：SSE 长连接 vs UDP 语音

---

## 7. 模拟追问清单（自测）

- [ ] 为什么 ChatGPT 用 SSE 而不是 WebSocket？
- [ ] 用户取消生成时 GPU 上发生什么？
- [ ] TPM 限流如何预估 prefill tokens？
- [ ] 语义缓存 0.95 阈值如何 A/B？
- [ ] Docker vs Firecracker 多租户结论？
- [ ] 100ms 沙箱启动具体数字路径？
- [ ] VAD 误触发如何不影响 Barge-in？
- [ ] 10 万相机为何不能全量上云？

---

## 8. 文档索引

| 题目 | 文档 |
|------|------|
| Q1–Q3 | [01-inference-api-platform.md](./01-inference-api-platform.md) |
| Q4 | [02-isolated-code-sandbox.md](./02-isolated-code-sandbox.md) |
| Q5–Q8 | [03-embedded-edge-multimodal.md](./03-embedded-edge-multimodal.md) · [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) |
