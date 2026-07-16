# 16 - Qualcomm AI Stack SDK 面试准备

面向 **Qualcomm AI Stack SDK Software** 团队 **Staff / Sr. Staff Software Engineer** 岗位（Generative AI inference on Snapdragon）。

> **GitHub 阅读：** 公式使用 `$...$` / `$$...$$` 格式。  
> **关联文档：** 量化/部署基础见 [07-端侧部署题详解.md](./07-端侧部署题详解.md)；Transformer 基础见 [06-CV基础题详解.md](./06-CV基础题详解.md)；**端侧吞吐/延迟/功耗硬核题库**见 [27-高通端侧LLM吞吐延迟功耗面试题详解.md](./27-高通端侧LLM吞吐延迟功耗面试题详解.md)。

---

## 一、岗位画像

**团队使命：** 设计、开发、交付面向 Snapdragon 平台的 GenAI 推理软件——模型优化、量化、图变换、Runtime 执行；支撑 QAIRT 及 ONNX Runtime / ExecuTorch / TFLite(LiteRT) delegate。

```
PyTorch / HuggingFace 模型
        ↓ 导出 (ONNX 等)
   计算图 IR + Graph Passes (fuse / fold / lower / quantize)
        ↓
   QAIRT / QNN Runtime + Framework Delegates
        ↓ 调度
   Snapdragon: CPU + Adreno GPU + Hexagon NPU (HTP)
        ↓
   Android / QNX 上的 LLM / LVM / LMM 应用
```

| 维度 | Staff/Sr. Staff 要求 |
|------|---------------------|
| 技术深度 | Transformer、量化、图编译、多 Runtime delegate |
| 工程广度 | Python 原型 + C/C++ 量产、CMake、跨层 debug |
| ownership | 特性从概念到量产 E2E；设计评审、代码评审 |
| 协作 | ML Research、HW/SW、PM、QA、全球客户/OEM |
| 领导力 | Mentor 初级工程师；与 Director+ 沟通路线图 |

**与 PICO Vision 岗差异：**

| PICO Vision Algorithm | Qualcomm AI Stack SDK |
|----------------------|------------------------|
| 算法指标 (mAP/PSNR) | 推理 KPI (tokens/s、延迟、内存、功耗) |
| 计算摄影 / CV 感知 | **LLM / LVM / LMM** 端侧推理 |
| TensorRT / NCNN | **QAIRT / QNN** + ORT/ExecuTorch/TFLite delegate |
| PyTorch 训练为主 | **导出 + 图变换 + Runtime** 为主 |

---

## 二、技能矩阵（Skill Matrix）

### 2.1 总览：JD → 技能 → 掌握标准

| JD 要求 | 技能域 | P0 掌握标准 | 关联复习 |
|---------|--------|-------------|----------|
| PyTorch/ONNX 转换部署 | 模型导出 | 能独立 export Llama block；处理 dynamic axes、unsupported op | 本文 §三 Q1–Q4 |
| Graph transformations | 图编译 | 能列举并解释 5+ graph passes；设计 fuse 方案 | 本文 §三 Q5–Q8 |
| Quantization & perf | 量化优化 | PTQ/QAT/W4A16；per-layer 误差分析；mixed precision 策略 | [07](./07-端侧部署题详解.md) + 本文 §三 Q9–Q12 |
| Transformer / Attention | GenAI 架构 | 手写 attention；讲清 prefill/decode、KV cache、GQA | [06](./06-CV基础题详解.md) + 本文 §三 Q13–Q17 |
| LoRA / MoE / Speculative | 新兴推理技术 | 白板上讲清对图、内存、延迟的影响 | 本文 §三 Q18–Q20 |
| Python + C/C++ | 工程能力 | 能用 C++ 调 QNN API；Python 写 graph pass 原型 | 本文 §四 |
| Debug 跨层问题 | 系统 debug | 有分层 RCA playbook（见 §五） | 本文 §三 Q21–Q23 |
| Android / RTOS | 嵌入式 | NDK、`adb` profiling；了解 QNX 场景 | 本文 §三 Q24 |
| QAIRT / QNN / NPU | Qualcomm 栈 | 能画 QAIRT 分层；解释 delegate 与 context binary | 本文 §三 Q25–Q27 |
| Staff 领导力 | 软技能 | 2+ STAR：E2E 特性、mentor、跨团队冲突 | 本文 §七 |

### 2.2 分项技能矩阵（自评用）

评分：**0** 不了解 · **1** 概念 · **2** 能讲 · **3** 能动手 · **4** 能带队交付

#### A. 推理与数值

| 技能项 | 0 | 1 | 2 | 3 | 4 | 备注 |
|--------|---|---|---|---|---|------|
| FP32/FP16/BF16/INT8/INT4 取舍 | | | | | | LLM 权重量化 vs 激活量化 |
| PTQ 校准 (MinMax/Entropy/Percentile) | | | | | | 见 [07 Q1–Q2](./07-端侧部署题详解.md) |
| QAT / Fake Quant / STE | | | | | | 见 [07 Q1](./07-端侧部署题详解.md) |
| per-tensor vs per-channel quant | | | | | | 卷积权重量化常用 per-channel |
| Mixed precision 敏感层策略 | | | | | | 首层/末层/LayerNorm/Attention |
| LLM weight-only (W4A16, AWQ/GPTQ 概念) | | | | | | Staff 岗常问 |
| 数值对齐 golden vs quant 逐层 diff | | | | | | debug 核心技能 |

#### B. 图与编译

| 技能项 | 0 | 1 | 2 | 3 | 4 | 备注 |
|--------|---|---|---|---|---|------|
| ONNX IR 与 opset 版本 | | | | | | 客户 issue #1 |
| Constant folding / DCE | | | | | | 基础 pass |
| Op fusion (Conv+BN+ReLU, LN+Linear) | | | | | | 面试高频 |
| Layout transform (NCHW/NHWC) | | | | | | NPU 常要求特定 layout |
| Graph lowering / legalization | | | | | | 高层 op → 后端原语 |
| Subgraph partition & delegate | | | | | | ORT EP / QNN delegate |
| Dynamic shape 与 recompile | | | | | | LLM seq_len 变化 |

#### C. Transformer / GenAI

| 技能项 | 0 | 1 | 2 | 3 | 4 | 备注 |
|--------|---|---|---|---|---|------|
| Scaled dot-product attention | | | | | | $O(n^2)$ 内存 |
| MHA / GQA / MQA | | | | | | KV cache 内存对比 |
| RoPE / RMSNorm / SwiGLU | | | | | | Llama 系标配 |
| Prefill vs Decode 阶段 | | | | | | compute-bound vs memory-bound |
| KV cache 形状与增长 | | | | | | 长上下文内存瓶颈 |
| Vision Encoder (ViT) + LMM | | | | | | 图像 token + 文本 token |
| LoRA adapter 编译/运行时 | | | | | | merge vs multi-adapter |
| MoE routing & 稀疏执行 | | | | | | 专家内存与负载均衡 |
| Speculative decoding | | | | | | draft + verify 加速比 |

#### D. Runtime & 平台

| 技能项 | 0 | 1 | 2 | 3 | 4 | 备注 |
|--------|---|---|---|---|---|------|
| ONNX Runtime EP 机制 | | | | | | CPU/CUDA/QNN |
| ExecuTorch delegation | | | | | | Meta edge 栈 |
| TFLite / LiteRT delegate | | | | | | 移动端传统路径 |
| QAIRT / QNN / Genie 分层 | | | | | | Qualcomm 差异化 |
| Hexagon NPU (HTP) 基本概念 | | | | | | 算子支持表驱动 |
| Android NDK on-device bench | | | | | | tokens/s, 内存峰值 |
| Context binary 缓存 | | | | | | 避免每次重编译 |

#### E. 工程与 Staff

| 技能项 | 0 | 1 | 2 | 3 | 4 | 备注 |
|--------|---|---|---|---|---|------|
| Python 生产级代码 & 测试 | | | | | | graph pass unit test |
| C/C++ 性能关键路径 | | | | | | RAII、内存池 |
| CMake 多 target 工程 | | | | | | SDK 标配 |
| Git workflow / Code Review | | | | | | |
| 设计文档 (RFC) | | | | | | Staff 必考 |
| Mentor 初级工程师 | | | | | | |
| 客户/OEM 问题 RCA | | | | | | |

### 2.3 备考优先级

| 优先级 | 内容 | 时间占比建议 |
|--------|------|--------------|
| **P0** | Transformer + KV cache + prefill/decode；PTQ/QAT；ONNX 导出/debug；图 pass 概念 | 50% |
| **P1** | ORT delegate；LLM 量化；QAIRT/QNN 文档；Android profiling | 30% |
| **P2** | LoRA/MoE/Spec decode；ExecuTorch；QNX；Staff 行为面 | 20% |

---

## 三、模拟面试 Q&A（技术深挖）

### 3.1 模型导出与 ONNX

#### Q1：把 PyTorch LLM 部署到 Snapdragon，端到端流程是什么？

**参考答案：**

```
1. 模型准备：HuggingFace 权重 → 确认算子可被目标 Runtime 支持
2. 导出：torch.onnx.export / torch.export → ONNX (固定 opset)
3. 图优化：shape inference → constant fold → fuse passes
4. 量化：校准集 PTQ 或 QAT；生成 quant params
5. 编译：QAIRT/QNN 将子图 lower 到 Hexagon；生成 context binary
6. 集成：通过 ORT QNN EP / Genie / 应用 SDK 加载
7. 验证：数值对齐 (perplexity / token match) + 性能 (prefill/decode latency)
8. 量产：错误处理、fallback、OTA 更新策略
```

**加分：** 标明哪些步骤在 **x86 主机**完成、哪些在 **on-device**完成。

---

#### Q2：ONNX 导出最常见的失败原因？怎么 debug？

| 失败类型 | 原因 | 处理 |
|----------|------|------|
| Unsupported op | `RotaryEmbedding`、自定义 `RMSNorm` 变体 | 替换为标准 op 组合或注册 custom op |
| Dynamic shape | `seq_len` 未声明为 dynamic axis | `dynamic_axes={'input': {1: 'seq'}}` |
| Opset 过高 | 目标 Runtime 不支持新 op | 降低 opset 或改图 |
| 权重过大 | 单 protobuf 超 2GB | `save_as_external_data` |
| 数值不一致 | 导出时 tracing 走了错误分支 | 固定 example input；对比 PyTorch vs ONNX 逐层输出 |

**Debug 步骤：** 最小子图复现 → `onnxruntime` 跑 FP32 golden → 二分定位出错层。

---

#### Q3：为什么推理图和训练图不一样？

| 训练 | 推理 |
|------|------|
| 需要 dropout、loss、backward | 去掉训练专用节点 |
| 动态 batch 训练 | 常为 batch=1 或固定 batch |
| 完整 attention 矩阵 | 可用 KV cache 增量 decode |
| FP32 为主 | FP16/INT8/INT4 混合 |
| 可变 seq（packing） | 常固定 max_seq 或分页 KV |

---

#### Q4：`torch.onnx.export` 和 `torch.export` 有什么区别？面试怎么答？

- **`torch.onnx.export`：** 传统 tracing/scripting 路径，生态成熟，LLM 上常需大量 workaround。  
- **`torch.export`：** PyTorch 2.x 新导出栈，基于 `ExportedProgram`，更利于后续 compiler 接入。  
- **面试要点：** 无论哪条路径，最终都要对齐 **ONNX opset + 目标 Runtime op support table**。

---

### 3.2 图变换与编译

#### Q5：什么是 graph lowering？举一个例子。

**答：** 将高层算子分解为后端支持的**原语算子**。

例：`LayerNorm` 在 ONNX 中可能是 `ReduceMean + Sub + Pow + Sqrt + Div` 组合；QNN 后端若有融合 `LayerNorm` kernel，则 lowering pass 将其**合并**为单节点以提升性能。

```
Graph Optimization Pipeline (典型顺序):
  Import → Shape Inference → Constant Fold → Canonicalize
        → Fusion → Quantization → Legalize → Partition → Compile
```

---

#### Q6：设计一个 Conv + BatchNorm + ReLU 融合 pass，要考虑什么？

| 考虑点 | 说明 |
|--------|------|
| 正确性 | 推理模式下 BN 参数 fold 进 Conv 权重：$W' = \gamma \cdot W / \sqrt{\sigma^2+\epsilon}$ |
| 训练 vs 推理 | 仅 inference graph 可 fold |
| 量化 | fold 后再 quant 通常更稳；注意 scale 传播 |
| 后端支持 | 融合后 op 是否被 NPU 支持；否则保持拆分 |
| 回退 | fusion 失败时保留原图 |

---

#### Q7：什么是 subgraph partition？delegate 怎么工作？

**答：**

1. Runtime 标记图中各节点所属 **Execution Provider**（CPU / GPU / NPU）。  
2. 连续 NPU 支持节点构成 **subgraph**。  
3. Subgraph **编译**为 NPU context（一次或缓存）。  
4. 执行时：不支持的 op **fallback** 到 CPU，产生 **设备间 copy** 开销。

**面试金句：** Partition 的目标是 **最大化 NPU 覆盖** 同时 **最小化 CPU↔NPU 往返**。

---

#### Q8：动态 shape 对 LLM 推理有什么影响？

| 场景 | 影响 |
|------|------|
| Prefill | `seq_len` 变化 → 不同 prompt 长度 |
| Decode | 每步 seq+1 → KV cache 增长 |
| 编译 | 固定 shape 可深度优化；动态需 bucketing 或 lazy recompile |
| 内存 | 需预留 `max_seq_len` 或 paged KV |

**策略：** Shape bucketing（128/256/512…）；或 Genie 类 runtime 内建 memory planner。

---

### 3.3 量化与性能

#### Q9：PTQ vs QAT，LLM 场景怎么选？

| 方法 | LLM 适用性 |
|------|-----------|
| **PTQ** | 权重量化 (W8/W4) 通常可行；激活 INT8 较难 |
| **QAT** | 精度要求高或激活量化时；成本高 |
| **Weight-only W4A16** | 端侧 LLM 主流折中：权重 INT4，激活 FP16 |
| **Mixed** | LayerNorm、Softmax、首尾层 FP16 |

**口述模板（延伸 [07 Q1](./07-端侧部署题详解.md)）：**

> LLM 我先做 weight-only PTQ 出 baseline，用 perplexity 和下游 task 验证；若敏感层掉点，对 embedding/output head 保留 FP16；仍不够再考虑 QAT 或 AWQ 类 outlier 处理。

---

#### Q10：哪些算子/层对 LLM 量化最敏感？

| 高敏感 | 原因 |
|--------|------|
| Embedding | 词表大，误差累积 |
| Output LM head | 直接影响 logits / token 选择 |
| LayerNorm / RMSNorm | 激活动态范围大 |
| Softmax in attention | 小扰动改变注意力分布 |
| 残差相加处 | 误差跨层累积 |

**处理：** FP16 保留、smoothquant 思想（重分配 scale）、per-channel weight quant。

---

#### Q11：如何 debug「量化后模型输出乱码」？

```
1. 确认 FP32 ONNX 与 PyTorch 对齐 (atol/rtol)
2. 若 FP32 已对：逐层量化，找 first divergent layer
3. 检查该层：scale 是否过小/过大、是否有 outlier channel
4. 尝试：该层 FP16、per-channel、换 calibration 方法
5. 检查：weight pack 格式、endianness、transpose 错误
6. 对比：CPU EP vs NPU EP，排除 backend bug
```

---

#### Q12：Prefill 和 Decode 的性能瓶颈分别是什么？

| 阶段 | 计算特征 | 典型瓶颈 | 优化方向 |
|------|----------|----------|----------|
| **Prefill** | 并行处理整个 prompt | Compute-bound (大 matmul) | 大 batch matmul、FlashAttention、NPU GEMM |
| **Decode** | 每步 1 token | Memory-bound (读 KV cache) | GQA/MQA、KV cache 量化、PagedAttention、spec decode |

**指标：**

- Prefill：**tokens/s**（吞吐）  
- Decode：**time-to-first-token (TTFT)** + **per-token latency**  

---

### 3.4 Transformer 与 GenAI 架构

#### Q13：写出 scaled dot-product attention 并解释复杂度。

$$
\mathrm{Attention}(Q,K,V) = \mathrm{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right) V
$$

- $Q,K,V$ shape：$(\text{heads}, \text{seq}, d_k)$  
- 朴素实现：$O(n^2 \cdot d)$ 时间，$O(n^2)$ 存 attention matrix  
- **FlashAttention：** IO-aware，减少 HBM 读写，不改变数学结果  

---

#### Q14：GQA 是什么？为什么 LLM 推理爱用？

**Grouped-Query Attention：** 多个 query head **共享**一组 K/V head。

| 类型 | Q heads : KV heads |
|------|------------------|
| MHA | H : H |
| GQA | H : G (G < H) |
| MQA | H : 1 |

**收益：** Decode 阶段 KV cache 内存 ∝ KV head 数 → GQA 显著省内存、省带宽。

---

#### Q15：KV cache 是什么？shape 怎么算？

Decode 时避免每步重算历史 token 的 K/V，缓存每层 attention 的 key/value。

单层单 batch cache 示意：

$$
\text{shape} \approx (\text{num\_kv\_heads},\; \text{seq\_len},\; \text{head\_dim})
$$

总内存（层数 $L$，batch $B$，每 KV bytes $b$）：

$$
\text{KV memory} \approx 2 \times L \times B \times \text{seq\_len} \times \text{kv\_heads} \times \text{head\_dim} \times b
$$

**8GB 手机跑 7B 模型：** KV cache 往往是长上下文的主要矛盾。

---

#### Q16：LMM（多模态大模型）推理 pipeline 怎么拆？

```
图像 → Vision Encoder (ViT/SigLIP) → image tokens
文本 → Tokenizer → text tokens
        ↓ concat / cross-attn
      Projector (MLP) 对齐维度
        ↓
      LLM backbone (prefill 图像+文本 prompt)
        ↓
      Autoregressive decode 生成回答
```

**部署难点：** 图像 encoder 算力大；image token 数影响 prefill 长度；端侧常 **量化 ViT + 小 LLM** 或 **云边协同**。

---

#### Q17：Vision Transformer (ViT) 和 CNN 比，部署上有什么不同？

| ViT | CNN |
|-----|-----|
| Patch embed + 大量 MatMul/Attention | 卷积为主，NPU 上成熟度高 |
| 序列长度 = patch 数 | 空间局部性强 |
| 对量化敏感（attention、LN） | 相对好 quant |
| 静态分辨率影响 token 数 | 多尺度更灵活 |

---

### 3.5 LoRA / MoE / Speculative Decoding

#### Q18：LoRA 原理？部署时有哪几种策略？

$$
W' = W + BA,\quad B \in \mathbb{R}^{d \times r},\; A \in \mathbb{R}^{r \times k},\; r \ll \min(d,k)
$$

| 策略 | 说明 |
|------|------|
| **Merge at compile** | $W' = W + BA$ 烘焙进权重；推理零开销；换 adapter 需重编译 |
| **Runtime adapter** | 保留 $BA$ 旁路；灵活切换；多一次 matmul |
| **Multi-LoRA batch** | 不同用户不同 adapter；batch 调度复杂 |

**PEFT / HuggingFace：** 训练时只更新 LoRA 参数；SDK 需支持客户 bring-your-own-adapter。

---

#### Q19：MoE 模型部署难点？

| 难点 | 说明 |
|------|------|
| 路由 | Top-k expert selection；负载不均 |
| 内存 | 全量 expert 权重巨大；常只加载 active experts |
| NPU | 稀疏、动态路由对静态图编译不友好 |
| 延迟 | 路由 + 多 expert 访问 → 不确定性 |

**面试答：** MoE 训练省 compute，推理未必省——端侧要关注 **active parameter** 和 **routing overhead**。

---

#### Q20：Speculative decoding 怎么加速？局限是什么？

```
Draft model (小) 快速生成 K 个候选 token
        ↓
Target model (大) 一次前向并行验证
        ↓
接受连续正确前缀；拒绝则回退
```

| 优点 | 局限 |
|------|------|
| 降低大模型 decode 步数 | 需额外 draft 模型内存 |
| 不改变输出分布（若实现正确） | 接受率依赖 draft 与 target 对齐度 |
| | 端侧要跑两个模型，工程复杂 |

---

### 3.6 Debug 与系统

#### Q21：客户报告 NPU 上结果错误，但 CPU 上正确，你怎么查？

1. 确认 ORT CPU EP FP32 golden  
2. 同一 quant 模型：CPU EP vs QNN EP 对比  
3. 若仅 QNN 错：最小子图 isolate op  
4. 查 op 实现：layout (NHWC)、accumulator 精度 (INT32)、fusion 边界  
5. 与 HW 团队确认 silicon errata / driver 版本  
6. 提供 workaround：disable 某 fusion 或 fallback 该 op 到 CPU  

---

#### Q22：如何设计 on-device LLM benchmark？

| 维度 | 指标 |
|------|------|
| Prefill | tokens/s @ prompt len {128,512,2048} |
| Decode | ms/token @ context {512,2k,8k} |
| Memory | 权重峰值 + KV peak + runtime scratch |
| Power |  sustained vs burst (热节流) |
| 正确性 | perplexity / 固定 prompt greedy decode match |

**工程：**  warmup runs、固定 CPU/NPU 频率（或注明未锁频）、多次取 p50/p99。

---

#### Q23：INT4 权重支持作为 SDK 新特性，设计评审要讲什么？

```
Problem：7B 模型 FP16 权重 ~14GB，端侧放不下
Goals：INT4 权重；精度损失 < X%；compile 时间 < Y min
API：量化配置 struct；compatible with existing QAIRT flow
Graph changes：weight pack format；dequant 或 native INT4 matmul
Testing：model zoo regression；numerical threshold；perf on 8 Gen 3
Rollout：feature flag；文档；migration guide
Risks：outlier channels；old chip 不支持 → fallback W8
```

---

### 3.7 平台与 Qualcomm 栈

#### Q24：Android 端侧推理要关注什么？

| 主题 | 要点 |
|------|------|
| NDK / JNI | Java/Kotlin app 调 native runtime |
| 内存 | `mmap` 大权重；避免 Java heap 拷贝 |
| 线程 | 推理线程与 UI 分离；Big/Little core 亲和 |
| 热节流 | 长时间 GenAI → 降频 → latency 变差 |
| 权限 | 模型文件存储、OTA 更新 |

---

#### Q25：QAIRT、QNN、Genie 分别是什么？

| 组件 | 角色 |
|------|------|
| **Hexagon NPU (HTP)** | Snapdragon 上 AI 加速器硬件 |
| **QNN (Qualcomm AI Engine Direct)** | 底层 NPU 编程接口；op 执行、memory、context |
| **QAIRT** | 统一 AI Runtime/SDK 品牌与工具链（转换、编译、执行） |
| **Genie** | 面向 GenAI（LLM）的高层 API / 运行时封装 |

**类比：** QNN ≈ CUDA driver level；QAIRT ≈ TensorRT + runtime；Genie ≈ llama.cpp / vLLM 的 Qualcomm 版生态位。

---

#### Q26：什么是 context binary？为什么重要？

编译 NPU 子图后生成的**二进制上下文**，包含优化后的 kernel 与权重布局。

| 好处 | 说明 |
|------|------|
| 启动快 | 跳过每次 compile |
| 确定性 | 量产版本一致 |
| OTA | 应用可 ship 预编译 binary |

**注意：** 不同 SoC generation / driver 版本可能不兼容。

---

#### Q27：和 TensorRT / NCNN 经验怎么迁移到 Qualcomm？

| 通用能力 | Qualcomm 对应 |
|----------|---------------|
| ONNX 导入 | QAIRT converter |
| Graph fusion | QNN graph optimizer |
| INT8 calibration | QNN quant tools |
| Engine/Context | QNN context binary |
| EP/Delegate | ORT QNN Execution Provider |

**面试话术：** 「我在 TensorRT 上做过 fusion 和 PTQ；同样方法论适用于 QAIRT——差异在 op support table 和 Hexagon memory layout。」

---

## 四、系统设计 mock

### SD1：在 Snapdragon 手机部署 Llama-3.2-1B 聊天应用

```
需求：离线对话；context 4k；首 token < 500ms； sustained 15+ tokens/s

架构：
  App (Android) → Genie/QAIRT Runtime
       → INT4 weight + FP16 activations
       → Prefill: NPU matmul + attention kernels
       → Decode: KV cache in dedicated buffer (FP16 or INT8 KV)
       → Tokenizer (CPU)

内存预算 (示意)：
  Weights INT4 1B ≈ 0.5–0.7GB
  KV @4k ≈ 数百 MB
  Runtime scratch ≈ 100–300MB
  → 需 memory planner + 可选 context sliding window

Fallback：NPU 不支持 op → CPU；模型降级到更小 variant
评测：固定 prompt suite；热节流 10min 后复测
```

---

### SD2：为 SDK 设计 ORT QNN EP 的 graph partition 策略

```
输入：ONNX model + QNN op support metadata
输出：Partition plan (NPU subgraphs + CPU fallback list)

算法要点：
  1. 拓扑排序遍历节点
  2. 标记每个节点：NPU_SUPPORTED / CPU_ONLY / UNKNOWN
  3. 合并连续 NPU 节点为 subgraph（受 max subgraph size 限制）
  4. 最小化 cut edges（避免频繁 DMA）
  5. 对 UNKNOWN：保守 fallback 或 pattern match

优化：
  - 插入 Q/DQ 节点满足 quant 约束
  - Layout cast 插入代价建模
  - Cache partition result per (model_hash, soc_gen)
```

---

### SD3：支持客户自带 LoRA adapter 的 SDK 特性

见 Q18 + 设计评审结构（Q23）；强调 **API 设计**、**安全**（adapter 校验）、**版本兼容**。

---

## 五、跨层 Debug Playbook

```
症状分类
├── Crash → stack trace → NPU driver / OOM / ABI mismatch
├── 数值错 → golden diff → 量化 vs FP32 → 单层 isolate
├── 变慢 → profile → NPU util / CPU fallback / excessive sync
└── 偶发 → race / thermal / dynamic shape recompile

工具链
  PyTorch compare → ONNX Runtime → QNN netron-like graph dump
  adb shell / systrace / Hexagon profiler (若可用)
  版本矩阵：SDK / driver / SoC / Android version
```

---

## 六、8 周备考计划

| 周 | 主题 | 产出 |
|----|------|------|
| 1 | Attention、MHA、GQA、RoPE；NumPy 实现 toy attention | 能白板推导 + 讲 KV cache |
| 2 | Prefill/decode；HuggingFace `generate()` profiling | 延迟分解表 |
| 3 | ONNX export 小 LM；debug unsupported op | 一份 export checklist |
| 4 | 精读 [07-端侧部署](./07-端侧部署题详解.md)；LLM W4A16 概念 | 量化决策树 |
| 5 | Graph passes 论文式总结；手写 constant fold + fusion 伪代码 | 5 个 pass 口述 |
| 6 | ORT EP 文档；delegate 机制；对比 CPU vs EP | 子图 partition 白板 |
| 7 | QAIRT/QNN 公开文档；Android on-device bench（有设备则实操） | SoC 软件栈一页纸 |
| 8 | 本文 §三 mock 全过；Staff STAR 3 则；模拟 2h 技术 loop | 弱项回补 |

---

## 七、行为面（Staff / Sr. Staff）

准备 **3 个 STAR**，每个 2–3 分钟：

| 主题 | 要点 |
|------|------|
| **E2E 特性交付** | 从 research 原型 → SDK API → 文档 → 客户集成 |
| **复杂 debug** | 跨 model/runtime/HW 分层 RCA；客户 deadline |
| **Mentor** | 帮 junior 定位 quant bug；优先级排序 |
| **技术分歧** | 与 HW 团队对 op support 范围谈判 |
| **全球协作** | 时差、异步 design review、清晰 ownership |

**Staff 信号词：** ownership、trade-off 文档化、可维护 API、测试覆盖、向后兼容。

---

## 八、推荐资源

| 资源 | 内容 |
|------|------|
| [Qualcomm AI Stack](https://www.qualcomm.com/developer/software/qualcomm-ai-stack) | QAIRT / 开发者文档入口 |
| ONNX Runtime docs | EP、量化、mobile |
| ExecuTorch docs | Edge delegation |
| HuggingFace PEFT | LoRA 训练与加载 |
| 本仓库 [07-端侧部署](./07-端侧部署题详解.md) | PTQ/QAT、TensorRT/NCNN 基础 |
| 本仓库 [06-CV基础](./06-CV基础题详解.md) | Transformer 入门 |

---

## 九、简历 Bullet 模板（本岗位）

> Owned end-to-end deployment of [LLM/ViT] on [Snapdragon X / mobile SoC]: PyTorch → ONNX → [QAIRT/ORT QNN EP], achieving [Y tokens/s] decode and [Z ms] prefill @ [context len], with [W4A16] quant and <[N]% perplexity regression vs FP16.

> Designed graph fusion pass for [LayerNorm+Linear], reducing kernel launch overhead by [X]% on Hexagon NPU.

> Led RCA for customer-reported numerical mismatch; isolated to [op name], shipped workaround in SDK [version].

---

## 十、自测清单（面试前 48h）

- [ ] 白板：attention + KV cache shape  
- [ ] 口述：PyTorch → QAIRT 全流程  
- [ ] 口述：PTQ vs QAT vs W4A16  
- [ ] 白板：graph partition / delegate  
- [ ] 口述：prefill vs decode 瓶颈  
- [ ] 口述：LoRA merge vs runtime  
- [ ] 口述：量化乱码 debug 步骤  
- [ ] 系统设计：端侧 1B LLM 聊天应用  
- [ ] STAR × 3（含 mentor 或 E2E）  
- [ ] 复习 [07](./07-端侧部署题详解.md) 量化 Q1–Q5  
