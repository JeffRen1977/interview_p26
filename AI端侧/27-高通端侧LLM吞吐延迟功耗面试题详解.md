# 27 - 高通端侧 LLM 吞吐 / 延迟 / 功耗面试题详解

面向 **Snapdragon / Hexagon NPU / Mobile** 端侧大模型推理岗位（Staff / Sr. Staff）。  
配套：[16-Qualcomm-AI-Stack面试准备.md](./16-Qualcomm-AI-Stack面试准备.md)（SDK / 图编译 / Runtime 栈）· [07-端侧部署题详解.md](./07-端侧部署题详解.md)（通用量化基础）。

> **答题四步法（Staff 金标准）：**  
> **痛点（Bottleneck） → 物理/数学公式 → 硬件权衡（HW Trade-off） → 落地效果（Impact）**  
> 不要只说「我们用了 INT4」——要说清楚为什么 Decoding 被 LPDDR 带宽卡住、4-bit 如何把访存压到 1/4、片上 dequant 如何把 memory-bound 转回 compute-bound。

---

## 目录

| 部分 | 内容 |
|------|------|
| [第一部分](#第一部分大模型端侧核心瓶颈与综合优化) | Prefill/Decode 权衡、系统级功耗 |
| [第二部分·一](#一现代大模型部署架构) | KV Cache / Spec Decode / MoE / LoRA / LMM Token 压缩 |
| [第二部分·二](#二编译器计算图优化与算子开发) | 算子融合、手写算子、Tiling |
| [第二部分·三](#三端侧深度量化) | 均匀量化、Outlier、AWQ/GPTQ、Mixed Precision |
| [第二部分·四](#四骁龙平台与系统级软件设计) | ION/DMA-BUF Zero-Copy、ExecuTorch+QNN Lowering |
| [速查卡片](#面试口述速查卡片) | 明天可背的 1 分钟口播 |

---

# 第一部分：大模型端侧核心瓶颈与综合优化

端侧优化 LLM 的本质，是解决两对矛盾：

1. **LPDDR 带宽墙**：手机 LPDDR5X 峰值带宽约 $50\text{–}80\,\mathrm{GB/s}$（远低于桌面 HBM），Decoding 几乎每一步都要把全部权重从 DRAM 搬一遍。  
2. **热/功耗墙**：持续 GenAI 会触发 Thermal Throttling，频率跌落后 tokens/s 腰斩。

---

## Q1. Prefill 与 Decoding 的硬件瓶颈有何本质不同？优化策略如何分化？

### 痛点

同一套权重、同一块 NPU，两个阶段的 **算术强度（Arithmetic Intensity）** 差一个数量级。

### 公式 / 物理

| 阶段 | 每 token 计算量（粗估） | 每 token 访存量 | 算术强度 |
|------|------------------------|-----------------|----------|
| **Prefill** | 对长度 $S$ 的 prompt 做并行 GEMM / Attention | 权重读一次 + 激活写回 | 高 → **Compute-bound** |
| **Decode** | 每步只产 1 个 token：$O(\text{params})$ FLOPs | 几乎整份权重 + 增长中的 KV | 极低 → **Memory-bound** |

Decoding 的理论吞吐上界（忽略算力）：

$$
\text{tokens/s} \approx \frac{\text{LPDDR Bandwidth}}{\text{Model Size in Bytes}}
$$

例：7B 模型 FP16 ≈ 14 GB；LPDDR 带宽 70 GB/s → 理想上限约 **5 tokens/s**。换成 W4（约 3.5 GB）→ 上限约 **20 tokens/s**。算力再强也破不了这条墙——除非少读 DRAM。

### Prefill 优化（打满 Tensor 单元）

| 策略 | 做法 | 为什么有效 |
|------|------|-----------|
| 大矩阵并行 | 把 prompt 整段当 batch 维喂进 HTP GEMM | 提高 MAC 利用率 |
| FlashAttention 级 tiling | Q/K/V 分块在片上 SRAM 完成 softmax 累加 | 避免 $O(S^2)$ attention 矩阵落 DRAM |
| 算子融合 | RMSNorm+QKV、GEMM+Bias+SiLU 融成一核 | 减少中间激活写回 |
| Shape bucketing | 128/256/512/2k 预编译 | 避免动态 shape 反复 recompile |

**Prefill KPI：** TTFT（Time-To-First-Token）、prompt tokens/s。

### Decode 优化（少碰 LPDDR）

| 策略 | 做法 | 为什么有效 |
|------|------|-----------|
| Weight-only 量化 | W4A16 / W4A8 | 权重体积 ÷4，带宽压力同比例下降 |
| KV 压缩 | GQA/MQA + KV INT8/FP8 | Decode 后期 KV 读带宽占比上升 |
| Speculative Decoding | 小草稿模型 + 大模型一次验证 | 把多次 memory-bound 步合成一次 compute-bound 验证 |
| 权重常驻 / 专家 Offload | 热权重留 SRAM/LPDDR，冷专家回 UFS | 减少无效搬运 |

**Decode KPI：** ms/token、sustained tokens/s（**热节流后**也要报）。

### 口述模板

> Prefill 是 compute-bound，我优化 NPU 张量吞吐和 FlashAttention 局部性；Decode 是 memory-bound，理论吞吐 ≈ 带宽/模型字节数。端侧我优先 W4 权重量化 + GQA + KV 量化，必要时再上 speculative decoding。两个阶段 KPI 分开报：TTFT vs ms/token，并且一定要测 sustained 热稳态。

---

## Q2. 如何从软件/算法减少 LPDDR 访问，降低功耗并缓解 Thermal Throttling？

### 痛点

手机跑 LLM 几分钟就降频。能量账本里，**DRAM 访问能耗通常是片上 SRAM 的几十到上百倍**；Decoding 每 token 都在扫权重 → 功耗 ∝ 访存量。

### 公式（数量级直觉）

$$
E_{\text{token}} \approx E_{\text{DRAM}}\cdot(\text{weights} + \text{KV}) + E_{\text{compute}}\cdot\text{FLOPs}
$$

Memory-bound 时第一项主导。把权重从 16-bit 压到 4-bit：

$$
\text{DRAM bytes} \downarrow 75\% \;\Rightarrow\; E_{\text{DRAM}} \downarrow \sim 75\%\ \text{（理想情况）}
$$

### 低功耗方案栈（由易到难）

| 层级 | 方案 | 机制 | 权衡 |
|------|------|------|------|
| 算法 | **Weight-only INT4/FP4** | 少读 75% 权重 | 需 AWQ/GPTQ 护 outlier；激活仍 FP16 |
| 算法 | **W4A8 / W8A8 共量化** | 走纯整型 MAC，少做 FP dequant | 激活 outlier 难；敏感层要 mixed precision |
| 编译 | **Layer-level Fusion** | RMSNorm+QKV、RoPE+QKV、GEMM+SiLU 片上完成 | 融合核难写；SRAM 容量受限需 tiling |
| 系统 | **DVFS / 热感知调度** | 突发高功率出首 token，稳态降频保持续 | 需与 Android thermal HAL 协同 |
| 系统 | **Big.LITTLE 亲和 + NPU 独占** | 推理线程绑大核、避免 CPU 抢带宽 | 功耗预算要与 UI 线程切分 |

### 为什么「片上融合」省电

未融合：`RMSNorm → 写 DRAM → 读 DRAM → QKV GEMM`，中间激活（常是 FP16，体积大）来回两次。  
融合后：激活留在 HTP 本地 SRAM / 寄存器堆，只把最终结果写回 → **少两次 LPDDR 往返**。

### 口述模板（Staff 级范例）

> 为把延迟和功耗做到极限，我们在骁龙上 profile 发现 Decode 阶段绝大部分能量耗在 LPDDR 搬权重。采用 **AWQ W4A16**：4-bit 权重把访存量压到约 1/4；激活保持 FP16 保住 attention 精度。NPU 侧手写 **fused on-the-fly dequant**：INT4 从 LPDDR 读进 SRAM 后在寄存器反量化再做 GEMM，把 memory-bound 转回接近 compute-bound。再配合 RMSNorm+QKV 融合减少激活落盘。结果通常是 tokens/s 明显上升，同时 sustained 功耗/温升下降——具体数字面试时用你项目里的实测替换。

---

# 第二部分：全套硬核面试问题

## 一、现代大模型部署架构

### Q3. [高频] 对比 MHA / GQA / MQA 的 KV Cache 内存；估算 Llama 3 70B（GQA 8:1）

#### 公式

每层每 batch 的 KV 字节数：

$$
\text{KV}_{\text{layer}} = 2 \times B \times S \times n_{\text{kv}} \times d_{\text{head}} \times b
$$

全模型：

$$
\text{KV}_{\text{total}} = L \times \text{KV}_{\text{layer}}
$$

| 类型 | $n_{\text{kv}}$ | 相对 MHA 的 KV 体积 |
|------|-----------------|---------------------|
| **MHA** | $= n_q$ | $1\times$ |
| **GQA** | $= n_q / g$（$g$ 为组大小） | $1/g$ |
| **MQA** | $= 1$ | $1/n_q$ |

#### Llama 3 70B 估算（口述数字）

典型配置量级：$L=80$，$n_q=64$，$n_{\text{kv}}=8$（8:1 GQA），$d_{\text{head}}=128$，FP16 则 $b=2$。

$$
\text{KV} \approx 80 \times 2 \times B \times S \times 8 \times 128 \times 2 = 655360 \cdot B \cdot S\ \text{bytes}
$$

| $B$ | $S$ | KV 约 |
|-----|-----|-------|
| 1 | 4096 | ≈ 2.5 GB |
| 1 | 8192 | ≈ 5.0 GB |
| 4 | 8192 | ≈ 20 GB |

**面试要点：** 70B 权重本身端侧放不下；这题考的是你 **会不会算 KV**，以及 GQA 相对 MHA 省 **8×** KV。端侧真正能跑的是 1B–8B + GQA + KV 量化。

---

### Q4. [追问] 8GB/12GB 手机如何撑 8k+ 上下文？（PagedAttention / KV Quant）

#### 痛点

连续预分配 `max_seq × layers × …` 会 **碎片 + 峰值内存爆炸**；8k 上下文下 KV 可占数 GB。

#### PagedAttention（vLLM 思想，端侧可裁剪）

- 把 KV 切成固定大小 **page/block**（如 16 tokens）。  
- 逻辑序列用 **block table** 映射到物理页，类似 OS 虚拟内存。  
- 好处：按需分配、减少内部碎片；多 session 可共享 prompt 前缀页（prefix caching）。  
- 端侧注意：NPU 更喜欢静态图 → 常用 **固定 page 池 + 运行时 remap**，而不是完全动态 malloc。

#### KV Cache Quantization

| 方案 | 压缩 | 风险 |
|------|------|------|
| KV INT8 | ~2× | 多数层可接受 |
| KV FP8 / INT4 | 2–4× | 长上下文注意力分布易漂；需 per-channel / per-token scale |
| 滑动窗口 / 流式注意力 | 上限固定 | 丢失远端依赖；适合对话摘要场景 |

**组合拳（手机 8k）：** GQA（已省） + KV INT8 + Paged 分配 + 必要时 windowed attention。  
例：若 FP16 KV 占 2 GB，INT8 → ~1 GB，再加分页避免一次性峰值。

---

### Q5. [追问] KV Cache Offloading：不活跃 KV 回 LPDDR，需要时 DMA 拉回 NPU

#### 设计要点

```
热 KV（最近 W 个 token / 当前层工作集）→ NPU 紧耦合 SRAM / 本地缓冲
冷 KV（更早前缀、非活跃 session）   → 系统 LPDDR（或更冷 → 压缩后存储）
召回路径：Runtime 发 DMA 描述符 → Hexagon DSP/NPU DMA → 零拷贝映射进计算
```

| 关键设计 | 说明 |
|----------|------|
| **驻留策略** | LRU / 最近窗口；prefilling 整段常热，decode 只热尾部 |
| **DMA 重叠** | 计算层 $i$ 时预取层 $i+1$ 的冷 KV（双缓冲） |
| **零拷贝** | 物理页用 ION/DMA-BUF 共享，避免 CPU memcpy |
| **一致性** | 写回前确保 NPU fence；读前 invalidate 相关 cache |

**权衡：** Offload 省片上内存，但多一次 DRAM 往返——只对 **真正冷** 的 KV 值得；热路径乱 offload 会更慢更耗电。

---

### Q6. [高频] 投机采样加速比公式；为何高 Batch 会衰减？

#### 公式

草稿模型每次提 $K$ 个候选，接受率期望为 $\alpha$（几何分布近似下平均接受长度 $\approx (1-\alpha^{K+1})/(1-\alpha)$ 等变体）。

一个常用近似：

$$
\text{Speedup} \approx \frac{1+\alpha+\alpha^2+\cdots+\alpha^{K}}{c_{\text{draft}}\cdot K + c_{\text{verify}}}
$$

其中 $c_{\text{draft}}$ 是草稿一步相对目标模型一步的代价比，$c_{\text{verify}}$ 是一次并行验证代价（常 ≈ 1 次大模型 forward）。

更直观：

$$
\text{Speedup} \approx \frac{\mathbb{E}[\text{accepted tokens per round}]}{\text{cost of one draft round + one verify}}
$$

#### 为何 High Batch / 高吞吐服务会衰减？

| 原因 | 解释 |
|------|------|
| Decode 从 memory-bound → 更偏 compute-bound | 大 batch 已能打满算力，少读几次权重的收益变小 |
| 验证步变重 | 大 batch × K 候选，attention/GEMM 更大 |
| 接受率不随 batch 提升 | $\alpha$ 由模型一致性决定，不帮你摊销验证成本 |
| 调度复杂 | 不同请求接受长度不同 → bubble |

**结论：** Spec decode 在 **小 batch / 端侧单用户** 最赚；服务端大 batch 要重新算 ROI。

---

### Q7. [端侧特化] 骁龙上如何权衡草稿模型大小 $t_{\text{draft}}$ 与接受率 $\alpha$？

#### 痛点

手机 Batch=1 时 memory-bound 最严重，spec decode 潜力最大；但草稿太大 → **抢 LPDDR 带宽**、占内存、拖垮 verify。

#### 权衡曲线（口述）

- 草稿太小（如 10–50M）：$t_{\text{draft}}$ 极低，但 $\alpha$ 低 → 总接受短，白跑验证。  
- 草稿太大（接近目标模型）：$\alpha$ 高，但 $K\cdot t_{\text{draft}}$ 本身已 memory-bound，净加速 → 0。  
- **甜点区：** 通常目标模型的 **5%–15% 参数量**，或同族蒸馏小模型；$\alpha$ 目标约 **0.6–0.8**（视任务）。

#### 骁龙落地建议

| 项 | 建议 |
|----|------|
| 草稿量化 | 可更激进（INT4/INT8），略损 $\alpha$ 换带宽 |
| 驻留 | 草稿权重尽量常驻；与目标模型 **错峰访问** LPDDR |
| $K$ | 端侧常 4–8；过大验证变重且拒绝浪费 |
| 退出 | 连续拒绝则降 $K$ 或暂时关闭 spec |

**口述金句：** 端侧优化的是 $\alpha / t_{\text{draft}}$ 比值，不是单独追高 $\alpha$。

---

### Q8. [高频] MoE：Token-Choice vs Expert-Choice；端侧专家 Offload

#### 路由对比

| | **Token-Choice**（常见） | **Expert-Choice** |
|--|--------------------------|-------------------|
| 做法 | 每个 token 选 Top-$k$ expert | 每个 expert 选 Top 容量内的 token |
| 负载 | 易倾斜，需 auxiliary loss / 限流 | 天然更均衡 |
| 端侧 | 实现简单，动态 shape 仍麻烦 | 容量约束利于静态缓冲规划 |

#### 端侧专家动态 Offloading（Ping-Pong + DMA）

```
时刻 t:  NPU 计算 Expert A（权重在 Buffer0）
         DMA 异步：把 Expert B 从 UFS/LPDDR → Buffer1
时刻 t+1: NPU 计算 Expert B（Buffer1）
         DMA 异步：预取 Expert C → Buffer0
```

| 组件 | 作用 |
|------|------|
| **双缓冲（Ping-Pong）** | 计算与搬运重叠，掩盖 UFS/LPDDR 延迟 |
| **Direct DMA** | 绕开 CPU memcpy；描述符队列敲门铃 |
| **LRU Expert Cache** | 热专家留在 LPDDR/NPU 可见内存；冷专家落 UFS |
| **路由预知** | 若能提前知道下一层 Top-k，可提前发 DMA（需要轻量 router 先行） |

**风险：** 路由突发导致 cache miss 风暴 → 延迟长尾；要限并发 miss、合并同一 expert 的 token。

---

### Q9. [高频] Multi-LoRA：Segmented BGEMM（Punica）

#### 痛点

同一 batch 内不同 token 挂不同 LoRA：若朴素 for-each-adapter 串行 GEMM → 吞吐崩。

#### Punica 思想

$$
y = xW + x(BA) = xW + (xB)A
$$

- 按 adapter id 把 batch 内 token **分段（segment）**。  
- 对共享同一 LoRA 的连续段做 **分块 BGEMM**：先算 $xB$，再算 $(\cdot)A$。  
- 基座 $W$ 仍可一次大 GEMM；LoRA 旁路用 segmented kernel 打满。

**端侧注意：** adapter 数量多时 segment 过碎 → 改用 **合并高频 LoRA** 或 **compile-time merge**（单用户场景直接 bake 进 $W$）。

---

### Q10. [高频] LMM Token 压缩：Perceiver Resampler vs Pixel Shuffle

#### 痛点

ViT 出 1024 个 image token → LLM prefill 变长、KV 爆炸。

| 方法 | 机制 | 输出 |
|------|------|------|
| **Perceiver Resampler** | 可学习 query 对 image token 做 Cross-Attention | 压到固定 $N$（如 64/256） |
| **Pixel Shuffle / 空间合并** | Reshape 合并邻域 patch + Linear 投影 | token 数按空间倍率下降 |

#### 端侧 NPU 谁更合适？

| 维度 | Perceiver | Pixel Shuffle |
|------|-----------|---------------|
| 算子 | Cross-Attn + Softmax，动态性强 | Reshape + GEMM，极规整 |
| NPU 友好度 | 中（attention 要专项核） | **高**（成熟 GEMM 路径） |
| 精度/表达 | 强，可学压缩 | 弱一些，偏启发式 |
| 延迟可预期性 | 一般 | **更好** |

**面试倾向答：** 端侧优先 **Pixel Shuffle / 空间合并 + MLP projector**（算子友好、易量化）；云侧或旗舰机对质量敏感再用 Perceiver。也可混合：浅层 shuffle 粗压 + 轻量 resampler。

---

## 二、编译器、计算图优化与算子开发

### Q11. [高频] 为何融合 Conv2D + BN + ReLU？省多少次访存？

#### 未融合数据流（示意）

```
X → Conv → Y1(写DRAM) → BN → Y2(写DRAM) → ReLU → Y3(写DRAM)
```

粗算：相对融合核，常 **多 2 次中间张量的整幅读写**（具体次数取决于实现是否已有 in-place）。

#### 融合后

- BN 的 $\gamma,\beta,\mu,\sigma$ **fold** 进 Conv 权重与 bias（推理模式）。  
- ReLU 跟在 MAC 后 **寄存器内** 完成。  
- 中间 $Y1,Y2$ 不落 DRAM → **省带宽、省功耗、升吞吐**。

LLM 同理，最值得融合的包括：

| 融合 | 收益点 |
|------|--------|
| RMSNorm + QKV Projection | 去掉 Norm 输出落盘 |
| RoPE + Q/K | 旋转在寄存器完成 |
| GEMM + Bias + SiLU/SwiGLU | 激活函数不读回 |
| Attention 内 QK^T + Softmax + PV（Flash 风格） | 巨幅减少中间矩阵 |

---

### Q12. [高频] 手写算子考点（口述要点）

> 完整代码见 [`interview_handwrite/`](../interview_handwrite/) 与 [04-手撕代码指南.md](./04-手撕代码指南.md)；面试重点讲清思路。

| 题 | 要点 |
|----|------|
| Zero-copy 虚拟边界卷积 | 不 pad 大图；用边界函数/指针夹紧；输出仍写连续 buffer |
| AVX2 / Neon 向量化 | 内层沿输出通道或宽度；注意对齐与尾数 scalar epilogue |
| OpenMP + SIMD | 外层并行行/块，内层 SIMD；避免 false sharing |
| NPU Tiling / GEMM | 见下题 |

---

### Q13. [追问] NPU Tiling：如何规划 L1/L2（Local SRAM）避免 GEMM thrashing？

#### 目标

计算 $C = A\times B$ 时，工作集永远落在片上，**不因错误块序反复换入换出**。

#### 经典三维分块

选择块大小 $T_m,T_n,T_k$ 使：

$$
\text{size}(A_{\text{tile}})+\text{size}(B_{\text{tile}})+\text{size}(C_{\text{tile}}) \le \text{SRAM capacity}
$$

| 层级 | 典型驻留 |
|------|----------|
| 最内（寄存器/L1） | 微内核累加器、$C$ 的小面板 |
| 中间（L2/TCM） | $A$ 面板 + $B$ 面板 |
| 外（DRAM） | 整矩阵；按 $K$ 向外积方向流式灌入 |

**防止 thrashing：**  
1) 先定 SRAM budget；2) 按 bandwidth-arithmetic 平衡选 $T_k$；3) 双缓冲下一 tile 的 DMA；4) 量化后 tile 字节数变小 → 可加大 $T_m/T_n$ 提高复用。

---

## 三、端侧深度量化

### Q14. [高频] 均匀量化：Scale $S$、Zero-point $Z$；对称 vs 非对称

$$
x_{\text{int}} = \mathrm{clamp}\Big(\mathrm{round}\big(\frac{x}{S}\big)+Z,\ q_{\min}, q_{\max}\Big)
$$

$$
\hat{x} = S\cdot(x_{\text{int}}-Z)
$$

| | **对称** | **非对称** |
|--|----------|------------|
| $Z$ | 通常 0 | 非 0，对齐实数 0 |
| 范围 | $[-a,a]$ | $[\min,\max]$ |
| 硬件 | 实现简单，MAC 友好 | 更贴激活分布，但计算多一次减 $Z$ |
| LLM 权重 | 常用对称 per-channel | 激活有时非对称 |

**定 $S$：** MinMax / Percentile / MSE；per-tensor vs **per-channel（权重主流）**。

---

### Q15. [高频] 激活 Outlier 如何毁掉 INT4/INT8？

- 少数通道出现极大值 → MinMax scale 被拉大 → **其余值量化分辨率崩塌**。  
- Softmax / Attention 对小扰动敏感 → 输出漂移、重复 token、乱码。  
- Weight-only 相对稳，是因为激活仍 FP16；**一做 W4A4 就必须处理 outlier**（SmoothQuant 迁移、AWQ 保护、mixed precision）。

---

### Q16. [高频] AWQ 原理；为何不用 GPTQ 的 Hessian？

#### 思想

**Activation-aware：** 看校准集上激活的平均幅度，找出对输出影响大的 **权重通道**（约 1% salient），对它们保护。

#### 做法（核心）

不直接把 salient 权重留 FP16（那样硬件不规则），而是对通道做 **等比缩放**：

$$
W' = W \cdot \mathrm{diag}(s),\quad X' = \mathrm{diag}(s)^{-1} X
$$

放大 salient 通道的权重幅度 → 量化网格相对更细；激活侧除回来保持数学等价（平滑到相邻层）。然后对 $W'$ 做常规 INT4。

#### 对比 GPTQ

| | **AWQ** | **GPTQ** |
|--|---------|----------|
| 核心 | 激活感知缩放 + 分块量化 | 二阶（Hessian）误差补偿 |
| 成本 | 低，校准快 | 需近似 $H^{-1}$，更重 |
| 端侧偏好 | **高**（工程简单、效果稳） | 精度极限场景 |

**口述：** AWQ 不求 Hessian，是因为用激活统计找到「贵权重」，用缩放在均匀量化下变相提高它们的有效比特。

---

### Q17. [追问] GPTQ：Cholesky 与 Lazy Batch Updates

- 目标：逐列（或逐块）量化权重，并用二阶信息把误差补偿到未量化权重。  
- Hessian $H \approx X^\top X$（对应该层输入）；需要反复用到 $H^{-1}$ 信息。  
- **Cholesky** 分解稳定地处理相关矩阵，支撑高效更新。  
- **Lazy Batch Updates：** 不是每量化一个标量就立刻更新全部剩余权重，而是攒一块再批量补偿 → 大幅减少内存流量、加速校准。

---

### Q18. [追问] Hexagon 只支持整型时，敏感层如何 Mixed Precision？QNN 如何调度？

#### 策略

| 层类型 | 建议 |
|--------|------|
| 大 GEMM（FFN、QKV） | INT8/INT4 主路径 |
| Softmax / 部分 Norm | **FP16 回退** |
| Embedding / LM Head | 常 FP16 或更高比特 |
| 残差汇合 | 注意累加精度（INT32 accum） |

#### QNN / QAIRT 视角（概念）

```
全图量化分析 → 标记敏感节点保持 FP
     ↓
Subgraph partition：INT 子图 → HTP 整型核；FP 子图 → FP16 核或 GPU/CPU
     ↓
插入 Quantize / Dequantize 边界节点（尽量融合进相邻核）
     ↓
生成 context binary；Runtime 按图调度，减少域切换次数
```

**金句：** Mixed precision 的性能关键不是「会不会回退」，而是 **回退边界能不能融合、CPU↔NPU 往返次数能不能压到接近 0**。

---

## 四、骁龙平台与系统级软件设计

### Q19. Android 多模态：Camera → NPU 如何 Zero-Copy？（ION / DMA-BUF / HardwareBuffer）

#### 痛点

Camera → CPU memcpy → GPU 纹理 → 再拷进 NPU = **带宽与功耗杀手**，还增加 TTFT。

#### 正确数据平面

```
Camera HAL
   ↓ 产出 GraphicBuffer / AHardwareBuffer
DMA-BUF fd（或遗留 ION）
   ↓ 导入
Adreno / Hexagon 均 map 同一物理页
   ↓
NPU 直接消费（可能需 CSC/裁剪在专用硬件或融合预处理核完成）
```

| 概念 | 要点 |
|------|------|
| **DMA-BUF** | Linux 标准缓冲共享；跨驱动传 fd |
| **ION（遗留）** | 高通早期连续内存分配器；新平台转向 DMA-BUF |
| **CMA** | Contiguous Memory Allocator：给需要大块物理连续内存的设备用 |
| **零拷贝条件** | 相同格式/stride 或硬件做 inline convert；CPU 不 touch 像素 |

**面试加分：** 谈 fence（acquire/release）、缓存一致性（CPU 不参与则可免）、以及为何 LMM 预处理（resize/normalize）应尽量做成 **NPU 图前缀算子** 而不是 CPU 算完再送。

---

### Q20. ExecuTorch × QNN Lowering 生命周期

```
PyTorch 模型
   ↓ torch.export → ExportedProgram（Aten 图）
ExecuTorch 前端：图分解 / 类型提升 / 元数据
   ↓
Delegation：标记可由 QNN 后端执行的子图
   ↓ AoT
QNN 编译器：op legalize → fusion → quant pack → Hexagon 目标码
   ↓ 产物
.pte / context .bin / .so（权重 + 已编译图）
   ↓ Runtime on device
QNN Context Create → 内存预分配 → 加载静态权重
   ↓
Set input (DMA-BUF) → Execute → Get output
```

| 阶段 | 关键动作 |
|------|----------|
| **AoT** | 尽量在主机完成重编译；设备只加载 binary |
| **Partition** | 最大化 HTP 覆盖，最小化 fallback 边界 |
| **Runtime** | 预分配 KV/activation workspace；避免推理中 malloc |
| **失败路径** | 不支持 op → CPU 委派；要有 profiling 统计 fallback 占比 |

**与 ORT QNN EP 对照：** 思想相同（子图委派 + context cache），ExecuTorch 更偏移动端包体与 AoT。

---

# 面试口述速查卡片

### 1 分钟：端侧 LLM 怎么优化？

> Decode 被 LPDDR 带宽卡住，tokens/s ≈ 带宽/模型字节数。所以第一刀是 **W4 权重量化（AWQ）** 把访存砍到约 1/4；第二刀 **GQA + KV INT8** 压长上下文；第三刀 **算子融合** 让激活少落 DRAM；单用户再上 **speculative decoding** 吃 memory-bound 红利。功耗跟访存量几乎同向，热节流要报 sustained 指标。

### KV 公式（默写）

$$
\text{KV} = 2 \cdot L \cdot B \cdot S \cdot n_{\text{kv}} \cdot d_{\text{head}} \cdot b
$$

### Spec 加速直觉

$$
\text{Speedup} \sim \frac{\mathbb{E}[\text{接受 token 数}]}{K\cdot t_{\text{draft}} + t_{\text{verify}}}
$$

端侧追 $\alpha / t_{\text{draft}}$，服务端大 batch 收益衰减。

### 量化一句话

> Outlier 毁激活量化；端侧主流 **W4A16 + AWQ**；敏感层 mixed FP16；整型通路靠融合 Q/DQ 边界。

### Zero-Copy 一句话

> Camera 的 AHardwareBuffer/DMA-BUF fd 直接 import 给 NPU，CPU 不碰像素；预处理进计算图。

---

## 明天面前提问清单（建议你主动抛）

1. 贵团队更关注 **Genie / QAIRT 产品化** 还是 **HTP 算子 / 编译器**？我可按侧重点展开。  
2. 目标模型量级是 **1–3B 手机助手** 还是 **7B+ 旗舰**？决定 W4 vs 更激进压缩、是否 MoE。  
3. KPI 更看 **TTFT、decode tokens/s，还是 sustained 功耗/热**？我用对应优化故事对齐。

---

## 关联复习

| 文档 | 用途 |
|------|------|
| [16-Qualcomm-AI-Stack面试准备.md](./16-Qualcomm-AI-Stack面试准备.md) | SDK 技能矩阵、导出/Delegate、行为面 |
| [07-端侧部署题详解.md](./07-端侧部署题详解.md) | PTQ/QAT 通用基础 |
| [26-Microsoft-Principal-ML-Systems面试准备.md](./26-Microsoft-Principal-ML-Systems面试准备.md) | 推理系统、并行与性能叙事 |
| [`interview_handwrite/`](../interview_handwrite/) | 卷积 / 环形缓冲 / SharedPtr 手撕 |
