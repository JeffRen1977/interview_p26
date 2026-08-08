# TLM Embedded Experiences — 4 周硬核备战计划

> **岗位：** TLM, Embedded Experiences (San Francisco) · Cooperative AI / Edge & Consumer Devices  
> **目标：** 1 个月内把「底层系统 + 边缘推理 + Agentic Runtime + TLM 领导力」对齐到可上白板的深度  
> **通关秘籍：** [04-tlm-interview-playbook.md](./04-tlm-interview-playbook.md)  
> **本仓设计文档入口：** [README.md](./README.md)

---

## 0. 职位画像（4 维考点）

Cooperative AI 把 LLM / Agents **从云端推向边缘、消费级硬件与嵌入式**。面试官同时看 Hands-on 深度与 TLM 判断力。

```
                    ┌────────────────────────────────────────┐
                    │  OpenAI TLM Embedded Experiences 画像  │
                    └───────────────────┬────────────────────┘
                                        │
         ┌──────────────────────────────┼──────────────────────────────┐
         ▼                              ▼                              ▼
┌─────────────────┐            ┌─────────────────┐            ┌─────────────────┐
│ 1. 底层系统与硬件│            │ 2. 边缘AI与推理  │            │ 3. TLM 领导力   │
│ • C++/Rust 编程 │            │ • 资源受限推理  │            │ • 快速迭代 vs 稳│
│ • 内存管理 & 线程│            │ • Agent 运行沙箱│            │ • 团队技术教练   │
│ • 跨端 I/O 编排 │            │ • 量化与执行引擎│            │ • 跨职能业务落地│
└─────────────────┘            └─────────────────┘            └────────┬────────┘
                                                                       │
                              ┌─────────────────┐                      │
                              │ 4. Agentic Runtime│◄────────────────────┘
                              │ • Durable / 弱网 │
                              │ • Guardrails     │
                              └─────────────────┘
```

| 维度 | 面试官在听什么 | 本仓主攻材料 |
|------|----------------|--------------|
| 系统 / 硬件 | 内存、线程、零拷贝、传感器 I/O | `interview_handwrite/` · Event Loop |
| 边缘推理 | Prefill/Decode、量化、KV、ExecuTorch | `docs/07` · 眼镜 Runtime · Qualcomm 题 |
| Agent 安全 | Durable、Sandbox、熔断 | 沙箱 · 边缘协同 · WAL/Outbox |
| TLM | Ambiguity、Research↔Eng、Coaching | 本计划第 4 周 + Playbook |

---

## 1. 四周总览

| 周 | 堡垒 | 产出标准（周末自检） |
|----|------|----------------------|
| **W1** | C++/系统：并发、内存、软硬交互 | 白板手撕 Event Loop / MemPool / SharedPtr；讲清 memory order |
| **W2** | Edge AI：引擎、量化、端侧 Prefill/Decode | 讲清 Delegate / INT4 / 端侧 KV / Spec Decode |
| **W3** | Agentic Runtime：Durable、Sandbox、Guardrails | 白板画出断电恢复 + Wasm 隔离 + 危险指令熔断 |
| **W4** | 系统设计 Mock + Behavioral + 源码谈资 | 眼镜 Runtime + 弱网协同各过一轮；Behavioral 故事就绪 |

每天建议：**上午读/写代码 2–3h，下午口述/白板 1h，晚上对照文档追问清单 30min。**

---

## 第 1 周：底层系统级功底（C++ / 并发 / 软硬）

*目标：白板与底层设计中，对内存、线程、系统资源调度无懈可击。*

### Day 1–2：高性能并发与多线程

| | 内容 |
|--|------|
| **复习** | Lock-free、线程池、条件变量、Memory Barriers / `memory_order` |
| **思考题** | 如何设计高性能异步 Event Loop，处理嵌入式多传感器 I/O？ |
| **本仓** | [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) · [sensor_event_loop.cpp](./sensor_event_loop.cpp) · [../interview_handwrite/spsc_ring_buffer.cpp](../interview_handwrite/spsc_ring_buffer.cpp) · [../interview_handwrite/mpmc_ring_buffer.cpp](../interview_handwrite/mpmc_ring_buffer.cpp) · [../amazon_cpp/docs/07-Linux系统与设计题.md](../amazon_cpp/docs/07-Linux系统与设计题.md)（ThreadPool） |
| **口述检查** | Active Poll vs Sleep；每源一条 SPSC；队列满丢最旧还是最新 |

### Day 3–4：内存管理与极致优化

| | 内容 |
|--|------|
| **复习** | SharedPtr 底层、RAII、Move、零拷贝路径上的泄漏/生命周期 |
| **思考题** | 有限 RAM 下，如何用自定义 Memory Pool 承载高频 Token / KV Block？ |
| **本仓** | [../interview_handwrite/shared_ptr.cpp](../interview_handwrite/shared_ptr.cpp) · [../interview_handwrite/object_pool.cpp](../interview_handwrite/object_pool.cpp) · [../interview_handwrite/two_level_mempool.cpp](../interview_handwrite/two_level_mempool.cpp)（含 ABA） · [../amazon_cpp/docs/09-无锁固定大小内存池.md](../amazon_cpp/docs/09-无锁固定大小内存池.md) |
| **口述检查** | Rule of Five；CAS/ABA；Local Cache + Global Treiber；dma-buf FD 谁持有引用 |

### Day 5–7：嵌入式与软硬交互

| | 内容 |
|--|------|
| **复习** | User/Kernel 边界；I2C / SPI / UART 在 OS 抽象；Linux/RTOS 调度 |
| **加练** | VAD/Wake IRQ → 主 SOC；Always-On 级联（为第 4 周眼镜题铺垫） |
| **本仓** | [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) §Always-On · [realtime-voice-assistant.md](./realtime-voice-assistant.md) §dma-buf · [../docs/05-系统设计题与模拟面试.md](../docs/05-系统设计题与模拟面试.md) |
| **口述检查** | 为何主 SOC 不能 Always-On；IRQ 路径禁 malloc；硬件时间戳对齐 |

**W1 周末小测（30min 白板）：** 画 Event Loop 架构 + 手写 SPSC push/pop 伪代码 + 口述两级 mempool ABA。

---

## 第 2 周：边缘 AI 推理与模型压榨

*目标：彻底讲清大模型如何塞进资源受限设备。*

### Day 8–9：边缘推理引擎

| | 内容 |
|--|------|
| **复习** | **ExecuTorch**、ONNX Runtime Mobile、CoreML 架构 |
| **攻关** | Operators **Delegates**（NPU/GPU）；编译期 vs 运行期 **Memory Planning** |
| **本仓** | [../docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md) · [../docs/16-Qualcomm-AI-Stack面试准备.md](../docs/16-Qualcomm-AI-Stack面试准备.md) · [../docs/27-高通端侧LLM吞吐延迟功耗面试题详解.md](../docs/27-高通端侧LLM吞吐延迟功耗面试题详解.md) |
| **口述检查** | Delegate 失败 fallback 到 CPU；静态内存规划为何适合眼镜 |

### Day 10–11：量化与加速

| | 内容 |
|--|------|
| **复习** | FP8 / INT8 / INT4 / AWQ 在 Runtime 的落点 |
| **硬核** | Weight-only vs 激活量化：算力 vs 带宽瓶颈；寄存器内 **Dequant** 如何发生 |
| **本仓** | 同上 docs/07、docs/27；[smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) §量化 |
| **口述检查** | ASR 为何 INT8、VLM 为何 INT4+FP16 激活；校准失败症状 |

### Day 12–14：端侧 LLM 全流程

| | 内容 |
|--|------|
| **复习** | Prefill（算力密）vs Decode（带宽密）在端侧的表现 |
| **技术点** | 端侧 **KV 压缩/驻留**；**Speculative Decoding**（小草稿 + 大验证） |
| **本仓** | [../LLM/](../LLM/)（KV / PagedAttention）· [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) §PagedAttention · [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) §Spec Decode · docs/27 |
| **口述检查** | 为何端侧禁 `malloc` 扩 KV；Block 预分配 + LRU；草稿命中率与功耗 |

**W2 周末小测：** 白板画出「Camera dma-buf → ExecuTorch NPU → INT4 VLM → Block KV」延迟预算表（到 ms）。

---

## 第 3 周：Agentic Runtime 与安全沙箱

*目标：Agent 能自主跑工具，且断电/弱网/恶意输出下仍安全。*

### Day 15–17：Durable Execution（状态持久化）

| | 内容 |
|--|------|
| **复习** | 多步 Tool Calls、数分钟任务：断电/断网后的 **Checkpoint & Recovery** |
| **核心** | 状态机；本地 KV（RocksDB/SQLite）；WAL 可重放 |
| **本仓** | [edge-collaborative-agent-runtime.md](./edge-collaborative-agent-runtime.md) §WAL · §Outbox · §Graceful Degradation |
| **口述检查** | Action 执行前先写 WAL；Snapshot + Replay；离线降级三态 |

### Day 18–19：端侧 Sandboxing

| | 内容 |
|--|------|
| **复习** | Agent 生成代码如何安全执行；**Wasm** 毫秒级轻量沙箱；用户态 vs 内核隔离 |
| **本仓** | [isolated-code-sandbox.md](./isolated-code-sandbox.md) · [sandbox_orchestrator.py](./sandbox_orchestrator.py) · [02-isolated-code-sandbox.md](./02-isolated-code-sandbox.md) |
| **口述检查** | 边缘为何偏 Wasm 而非 MicroVM；default-deny；cgroups/超时仍适用吗 |

### Day 20–21：Runtime Guardrails（实时拦截）

| | 内容 |
|--|------|
| **复习** | 推理路径上硬规则层：危险指令（删文件、耗电攻击）**毫秒级熔断** |
| **设计点** | 输出流式扫描 + Circuit Breaker；与模型「软对齐」分层 |
| **本仓** | 沙箱 default-deny 思想迁移到工具白名单；[realtime-voice-assistant.md](./realtime-voice-assistant.md) Cancel/Barge-in 作「中断」类比 |
| **口述检查** | Guardrail 放 Prefill 后 / 每 Token / Tool 调用前哪一层；误杀如何降级 |

**W3 周末小测：** 白板「弱网协同 Agent」：Outbox → CRDT vs Quorum vs Lease 三层一致性 + 恢复防洪峰。

---

## 第 4 周：TLM 领导力与系统设计 Mock

*目标：对齐 OpenAI 文化；系统设计与 Behavioral 双线可上场。*

### Day 22–24：AI 嵌入式系统设计 Mock

| 模拟题 | 标准答案 | 复盘标准 |
|--------|----------|----------|
| **1.** 智能眼镜多模态、低延迟、省电 Agent Runtime | [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) | 数据流、硬件边界、算力/存储折中表、Fault Tolerance |
| **2.** 分布式边缘弱网断线重连协同控制 Agent | [edge-collaborative-agent-runtime.md](./edge-collaborative-agent-runtime.md) | QUIC/Outbox、CRDT+Quorum、Lease、Delta+QoS |
| **加练** | 语音全链路 / 相机级联 / Event Loop | [realtime-voice-assistant.md](./realtime-voice-assistant.md) · [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) · [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) |

每题按 Playbook：**Clarify → Diagram → Deep Dive → Failure → 10x/100x**。

### Day 25–27：Behavioral（TLM）

提前写好 **STAR** 卡片（每题 2–3min 口述）：

| 主题 | 准备角度 |
|------|----------|
| **快速迭代 vs 长期稳健** | Research 激进模型更新 vs Eng 稳定性：灰度、feature flag、回滚、SLA 护栏 |
| **Ambiguity / 0→1** | 端侧 Agent 无行业标准时，如何带 10–20 人定原则（约束优先、一致性分层、可测指标） |
| **技术教练** | 天赋强但偏执的底层工程师：用数据/边界条件纠偏，保留深度贡献 |
| **跨职能落地** | 半导体/多媒体经验 ↔ Agent 产品：把功耗/热/内存墙翻译成产品决策 |

**个人契合点（面试主动织入）：** PhD 深度 · Qualcomm 类硬核系统/多媒体 · Google 带 20+ 人 · 多智能体编排探索 → 强调 **「底层半导体/系统」×「前沿 Agent 架构」** 的结合，而非纯 Manager 叙事。

### Day 28–30：冲刺与技术对齐

| | 内容 |
|--|------|
| **源码精读（选一深挖）** | `vLLM` Scheduler（continuous batching / preemption）或 `ExecuTorch` runtime core（delegate / memory planning）— 变成可随口引用的细节 |
| **模拟面试** | 至少 2 轮：1× System Design，1× Behavioral |
| **通读** | [04-tlm-interview-playbook.md](./04-tlm-interview-playbook.md) 全文 + 本仓 Embedded 全部「口述 3 分钟版」 |

---

## 2. 通关建议（上场心态）

1. **不要像纯 Manager 面试。** OpenAI 极看重 **Hands-on**。多聊指针、Cache Line、NPU 算子/Delegate、KV Block、dma-buf — 比宏观管理理论更打动人。  
2. **用你的独特交叉优势。** 把半导体/多媒体硬核经验与 Agent Runtime 认知绑在同一条故事线：约束来自物理，架构来自 Agent。  
3. **每题先钉物理边界。** 功耗、RAM、RTT/分区、TTFT — 写在白板上再画框。  
4. **No Buzzwords。** 说了 ExecuTorch / CRDT / Raft / Wasm，必须能讲失效模式（对照 Playbook §3）。

---

## 3. 每日/每周检查表（可打印）

### 按周勾选

- [ ] W1：Event Loop + MemPool + SharedPtr 均可白板  
- [ ] W2：Prefill/Decode + INT4 + Delegate + 端侧 KV 口述通顺  
- [ ] W3：WAL 恢复 + Wasm/沙箱 + Guardrail 熔断路径清晰  
- [ ] W4：眼镜题 + 弱网协同题各完整 Mock 一遍；3 个 Behavioral STAR 就绪  

### 上场前 24h

- [ ] 重读两篇完整设计：眼镜 Runtime、边缘协同  
- [ ] 过一遍 Playbook Scaling Drill  
- [ ] 准备 1 个「我亲手改过的 cache-line / lock-free / NPU」细节故事  

---

## 4. 文档与 Demo 速查（按周）

| 周 | 设计文档 | 可跑 Demo / 手撕 |
|----|----------|------------------|
| W1 | Event Loop | `sensor_event_loop.cpp` · SPSC/MPMC · SharedPtr · two_level_mempool |
| W2 | 眼镜 Runtime · hybrid · docs/07·27 | —（以口述+算账为主） |
| W3 | 边缘协同 · 沙箱 | `sandbox_orchestrator.py` |
| W4 | 全部 + Playbook | 语音 / 相机 / 流式 Chat 任选 1 道加练 |

```bash
cd openai
python3 sensor_event_loop.py
python3 sandbox_orchestrator.py
python3 voice_assistant_pipeline.py
python3 hybrid_sensor_routing.py
cmake -S . -B build && cmake --build build --target sensor_event_loop && ./build/sensor_event_loop
```
