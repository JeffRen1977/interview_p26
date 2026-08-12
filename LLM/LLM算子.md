为了让你对这些算子在 LLM 中的实际位置和整体架构一目了然，下面为你梳理现代主流开源大模型（如 **LLaMA 2/3、Qwen 2、DeepSeek** 等）标准的 **Decoder-Only Transformer** 架构全貌。

---

## 一、 LLM 整体架构与算子分布图

数据在 LLM 内部从输入 Token 到输出下一个 Token 的流向及对应算子分布如下：

```
                   [ 输入 Token 序列 (Input Tokens: "Hello world") ]
                                         │
                                         ▼
                      [ Embedding Layer (词嵌入层) ]
                                         │
                                         ▼  x = Embedding(Tokens)
 ┌──────────────────────────────────────────────────────────────────────────────┐
 │  LLM Backbone (由 N 个相同的 Transformer Block 堆叠而成，如 32/80 层)          │
 │                                                                              │
 │    ┌────────────────────────────────────────────────────────────────────┐    │
 │    │  1. RMSNorm 算子 (Pre-Normalization)                               │    │
 │    │     x_norm = RMSNorm(x)                                            │    │
 │    └────────────────────────────────┬───────────────────────────────────┘    │
 │                                     │                                        │
 │                                     ▼                                        │
 │    ┌────────────────────────────────────────────────────────────────────┐    │
 │    │  2. Self-Attention 模块 (注意力层)                                  │    │
 │    │     ├── Linear 投影: Q = x_norm · W_q,  K = x_norm · W_k,  V = ...   │    │
 │    │     ├── RoPE 算子:  Q_rot = RoPE(Q),   K_rot = RoPE(K)              │    │
 │    │     ├── FlashAttention 算子 (包含 Scaled Dot-Product & Softmax):    │    │
 │    │     │   Attn_Out = Softmax( Q_rot · K_rot^T / sqrt(d_k) + Mask ) · V │    │
 │    │     └── Linear 投影: Out = Attn_Out · W_o                          │    │
 │    └────────────────────────────────┬───────────────────────────────────┘    │
 │                                     │                                        │
 │                                     ▼                                        │
 │                            [ + ] 残差连接 1 (x = x + Out)                      │
 │                                     │                                        │
 │                                     ▼                                        │
 │    ┌────────────────────────────────────────────────────────────────────┐    │
 │    │  3. RMSNorm 算子 (Pre-Normalization)                               │    │
 │    │     x_norm2 = RMSNorm(x)                                           │    │
 │    └────────────────────────────────┬───────────────────────────────────┘    │
 │                                     │                                        │
 │                                     ▼                                        │
 │    ┌────────────────────────────────────────────────────────────────────┐    │
 │    │  4. SwiGLU FFN / MLP 模块 (前馈神经网络层)                           │    │
 │    │     ├── Gate & Up 投影: Gate = x_norm2 · W_gate,  Up = x_norm2 · W_up│    │
 │    │     ├── SwiGLU 激活算子:  Act = SiLU(Gate) ⊙ Up                    │    │
 │    │     └── Down 投影:      FFN_Out = Act · W_down                     │    │
 │    └────────────────────────────────┬───────────────────────────────────┘    │
 │                                     │                                        │
 │                                     ▼                                        │
 │                            [ + ] 残差连接 2 (x = x + FFN_Out)                 │
 └─────────────────────────────────────┬────────────────────────────────────────┘
                                       │ (循环重复 N 层)
                                       ▼
                     [ 最终层 RMSNorm (Final Norm) ]
                                       │
                                       ▼
                     [ LM Head (Linear 投影到词表大小) ]
                                       │  Logits
                                       ▼
             [ Temperature Scaling & Top-k / Top-p 采样算子 ]
                                       │
                                       ▼
                   [ 输出下一个 Token (Next Token) ]

```

---

## 二、 架构与算子的分层拆解

整个大模型可以分为 **输入层**、**重复堆叠的 Transformer Block 核心层** 以及 **输出解码层** 三大部分：

### 1. 输入层（Input Phase）

* **Token Embeddings**：将输入的词 ID 转换为高维连续向量（如 $\mathrm{dmodel} = 4096$ 或 $8190$）。

### 2. 核心 Block 层（堆叠 32 ~ 80 层不等）

每一个 Block 内部包含两大核心子层，均采用 **Pre-LN（预归一化）** 和 **残差连接（Residual Connection）**：

#### **子层 A：自注意力子层 (Self-Attention Sub-layer)**

1. **RMSNorm**：首先对输入的隐藏状态 $x$ 做归一化，保证输入数据数值稳定。
2. **Q, K, V 线性投影**：将特征映射到多个 Head 上。
3. **RoPE（旋转位置编码）**：直接作用于 $Q$ 和 $K$ 向量，注入序列中 Token 的相对位置信息。
4. **FlashAttention（核心计算算子）**：
* 计算 $Q$ 和 $K$ 的点积相关度。
* 加上 **Causal Mask**（因果掩码，确保当前 Token 只能看到之前的 Token，看不到未来的 Token）。
* 经过 **Softmax** 转化为概率。
* 与 $V$ 相乘得到上下文融合特征。


5. **残差相加**：将自注意力的输出加回输入 $x$ 上：$x = x + \text{Attention}(RMSNorm(x))$。

#### **子层 B：前馈网络子层 (FFN / MLP Sub-layer)**

1. **RMSNorm**：再次对特征做归一化。
2. **SwiGLU 升维与激活**：
* 输入分成两路，分别经过 $\mathrm{Wgate}$ 和 $\mathrm{Wup}$ 投影升维（通常升维到约 $8/3 \times \mathrm{dmodel}$）。
* 一路经过 **SiLU (Swish)** 激活函数后，与另一路按元素相乘（$\odot$）。


3. **Down 降维投影**：通过 $\mathrm{Wdown}$ 将维度降回 $\mathrm{dmodel}$。
4. **残差相加**：将 FFN 的输出加回输入 $x$ 上：$x = x + \text{FFN}(RMSNorm(x))$。

### 3. 输出与采样层（Output & Sampling Phase）

1. **Final RMSNorm**：在所有 Block 执行完毕后，做最后一次特征归一化。
2. **LM Head**：通过一个线性层，将 $\mathrm{dmodel}$ 维度的特征映射到词表大小（Vocabulary Size，如 32,000 或 151,646），得到每个词的 raw score（Logits）。
3. **采样算子（Sampling Operators）**：
* **Temperature Scaling**：除以温度系数调节概率平滑度。
* **Top-k / Top-p (Nucleus) 过滤**：截断低概率词。
* **Softmax**：转为概率分布后随机/贪婪采样出下一个 Token。



---

## 三、 现代 LLM 架构演进趋势（面试高频）

如果你在面试中聊到 LLM 架构，提到以下几点演进会非常加分：

1. **Pre-Norm 替代 Post-Norm**：早期的 Vaswani Transformer 使用 Post-Norm（先算 Attn 再做 Norm），深层容易梯度爆炸/消失；现代 LLM 统一改用 **Pre-Norm**（先做 Norm 再算 Attn），训练极其稳定。
2. **RMSNorm 替代 LayerNorm**：去掉了计算均值的步骤，计算更高效，适合 GPU/NPU 算子融合。
3. **SwiGLU 替代 Gated GELU/ReLU**：虽然增加了参数量和一次投影计算，但表达能力显著增强。
4. **RoPE 替代 绝对/相对位置编码**：旋转位置编码直接在复数/二维平面做旋转变换，对长文本扩展（Context Window Extension）非常友好。
5. **GQA (Grouped-Query Attention) 替代 MHA**：在 Attention 部分，Query 依然保留多头，但 Key 和 Value 多个 Head 共享一组，**大幅减少推理生成阶段（KV Cache）的显存占用与访存带宽**。
