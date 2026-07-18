# OpenAI 系统设计面试（TLM / Embedded Experiences）

面向 **OpenAI TLM（Tech Lead Manager）** 及 **Embedded Experiences** 等工程落地、软硬件协同、高并发大模型应用岗位。

OpenAI 系统设计与大厂传统「Design Twitter / TinyURL」有本质区别，遵循三原则：

| 原则 | 含义 |
|------|------|
| **Model-Aware** | 设计必须围绕 Token 经济学、GPU 硬延迟、Prefill/Decode 差异 |
| **Ambiguity** | 需求模糊时主动澄清 SLA、成本、降级策略 |
| **Deep Dive Into Internals** | 每个组件要能讲清底层原理与失效模式 |

> **关联：** 推理工程手撕 → [../LLM/](../LLM/) · 端侧部署 → [../docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md) · 系统设计通用框架 → [../docs/05-系统设计题与模拟面试.md](../docs/05-系统设计题与模拟面试.md)

## 文档目录

| 文档 | 题目 |
|------|------|
| [streaming-chatgpt-backend.md](./streaming-chatgpt-backend.md) | **完整设计：** 实时流式 ChatGPT 后端（SSE/WebRTC、RPS/TPS、TTFT、Barge-in） |
| [llm-rate-limiter.md](./llm-rate-limiter.md) | **完整设计：** 多租户 RPM+TPM 限流（动态权重令牌桶、Local Cache + 异步 Flush） |
| [semantic-cache-rag.md](./semantic-cache-rag.md) | **完整设计：** 语义缓存 RAG（Cosine、TTL、批量 Invalidate） |
| [isolated-code-sandbox.md](./isolated-code-sandbox.md) | **完整设计：** 多租户代码沙箱（Docker vs MicroVM、100ms 冷启动、cgroups） |
| [realtime-voice-assistant.md](./realtime-voice-assistant.md) | **完整设计：** 实时语音助手（WebRTC/FEC、Barge-in、dma-buf、300ms 预算） |
| [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) | **完整设计：** 10 万相机混合路由（INT4 端侧、置信度级联、Embedding 上云） |
| [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) | **完整设计：** 多传感器异步事件循环（SPSC、poll/sleep、时间戳对齐） |
| [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) | **完整设计：** 智能眼镜 Agent Runtime（Always-On 唤醒、dma-buf、Hybrid 路由） |
| [01-inference-api-platform.md](./01-inference-api-platform.md) | 流式摘要 · Token 速率限制 · 语义缓存 RAG |
| [02-isolated-code-sandbox.md](./02-isolated-code-sandbox.md) | 沙箱摘要（详见 isolated-code-sandbox.md） |
| [03-embedded-edge-multimodal.md](./03-embedded-edge-multimodal.md) | 语音/边缘摘要（详见 realtime / hybrid 完整设计） |
| [04-tlm-interview-playbook.md](./04-tlm-interview-playbook.md) | TLM 高分通关：No Buzzwords · 10x/100x Scaling Drill |

## 参考实现（手撕 Demo）

| 文件 | 内容 |
|------|------|
| `streaming_chat_demo.py` | SSE 风格 token 推送 + barge-in cancel |
| `token_rate_limiter.py` | 双维度令牌桶 RPM+TPM · reconcile · Local shard + async flush |
| `semantic_cache.py` | 语义缓存：cosine 命中 · TTL · kb_version · 邻域批量失效 |
| `sandbox_orchestrator.py` | 沙箱编排：Warm Pool · Snapshot · default-deny · timeout |
| `voice_assistant_pipeline.py` | 语音助手：延迟预算 · FEC · Barge-in/AEC · dma-buf |
| `hybrid_sensor_routing.py` | 相机舰队：置信度级联 · Embedding 上云 · 带宽对比 |
| `sensor_event_loop.py` | 多传感器 Event Loop：SPSC · poll/sleep · IMU↔Camera 对齐 |
| `sensor_event_loop.cpp` | 同上 C++ 版（atomic SPSC + condition_variable 休眠） |

```bash
cd openai
python3 streaming_chat_demo.py
python3 token_rate_limiter.py
python3 semantic_cache.py
python3 sandbox_orchestrator.py
python3 voice_assistant_pipeline.py
python3 hybrid_sensor_routing.py
python3 sensor_event_loop.py

# C++ Event Loop demo
cmake -S . -B build && cmake --build build --target sensor_event_loop
./build/sensor_event_loop
```

## 三大方向速查

```
方向一  Inference & API Platform     → 01
方向二  Security & Execution Sandbox  → 02
方向三  Embedded / Edge / Multimodal  → 03
```

## 备考优先级（Embedded Experiences）

| 优先级 | 内容 |
|--------|------|
| **P0** | Q5 实时语音全链路（WebRTC、Barge-in、VAD/AEC） |
| **P0** | 多传感器 Event Loop（SPSC、省电 sleep、时间对齐） |
| **P0** | 智能眼镜 Runtime（Always-On、零拷贝、Hybrid 路由） |
| **P0** | Q1 流式 SSE、TTFT、TPS vs RPS |
| **P1** | Q4 MicroVM 沙箱、冷启动、cgroups |
| **P1** | Q2 Token Bucket RPM/TPM、Q3 语义缓存 |
| **P2** | Q6 端云级联、小模型本地过滤 |
