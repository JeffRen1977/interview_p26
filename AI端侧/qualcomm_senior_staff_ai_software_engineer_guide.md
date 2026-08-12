# Qualcomm Senior Staff AI Software Engineer Interview Guide

> **GitHub 公式注意：** 站点数学引擎里 `_` 极易破坏渲染。下文公式使用无下划线命名（如 `Tideal`、`HKV`）；带下划线的工程名写在 `text` 代码块里。
### Advanced AI Systems, Qualcomm AI Stack (QNN/AIMET), Hexagon NPU (HTP), On-Device LLM/GenAI, High-Performance C++ Coding & Numerical RCA

---

## 目录 (Table of Contents)
1. [Senior Staff AI 岗位定位与技术图谱](#1-senior-staff-ai-岗位定位与技术图谱)
2. [模块一：高阶 C++ / AI 系统级 Coding 真题与源码](#2-模块一高阶-c--ai-系统级-coding-真题与源码)
   - 2.1 题目一：INT8 对称/非对称定点量化与反量化算子 (Quantize / Dequantize Kernel with Saturation & Scale-ZeroPoint)
   - 2.2 题目二：端侧 LLM 分页 KV Cache 内存管理器 (Paged KV Cache Manager with Page Table & Block Allocation)
   - 2.3 题目三：VTCM / SRAM 片上内存分块矩阵乘法 (Tiled Matrix Multiplication with DMA Ping-Pong Buffer Simulation)
   - 2.4 题目四：端侧生成式 Top-K / Top-P (Nucleus) 采样算法 (Efficient C++ Sampling Engine)
   - 2.5 题目五：计算图算子融合与重写引擎 (Operator Fusion & Graph Optimization: Conv2D + BN + ReLU)
3. [模块二：Qualcomm AI Stack 软件架构与 QNN SDK 深度剖析](#3-模块二qualcomm-ai-stack-软件架构与-qnn-sdk-深度剖析)
   - 3.1 QNN vs. SNPE 演进与统一 C API 架构 (`QnnContext`, `QnnGraph`, `QnnTensor`)
   - 3.2 离线编译与 Context Binary 生成 (`qnn-context-binary-generator`)
   - 3.3 零拷贝内存注册 (`QnnMem_t` 与 Linux `dma-buf`) 及 FastRPC 通信机制
   - 3.4 异构图切分与多后端 Fallback 策略 (HTP vs. GPU vs. CPU)
4. [模块三：Hexagon NPU / HTP 硬件架构与底层加速原理](#4-模块三hexagon-npu--htp-硬件架构与底层加速原理)
   - 4.1 HTP 计算核心拓扑：HVX (1024-bit SIMD) 与 HMX (Matrix Engine) 协同
   - 4.2 VTCM (Vector Tightly-Coupled Memory) 片上 SRAM 管理与双缓冲 (Double Buffering)
   - 4.3 权重量化格式排布与内存访问局部性 (Weight Transformation for HTP)
5. [模块四：模型量化、压缩与 AIMET 工具链体系](#5-模块四模型量化压缩与-aimet-工具链体系)
   - 5.1 Post-Training Quantization (PTQ) 高阶方案：CLE (Cross-Layer Equalization) 与 Bias Correction
   - 5.2 AdaRound (Adaptive Rounding) 数学推导与损失函数设计
   - 5.3 激活值离群点 (Outliers) 处理策略：SmoothQuant 与 AWQ 原理
   - 5.4 Quantization-Aware Training (QAT) 与伪量化 (Fake Quantization) 节点
6. [模块五：端侧大模型 (On-Device LLM & GenAI) 部署与优化体系](#6-模块五端侧大模型-on-device-llm--genai-部署与优化体系)
   - 7.1 Prefill (GEMM 算力受限) vs. Decode (GEMV 带宽受限) Roofline 模型推导
   - 7.2 端侧内存带宽瓶颈计算：7B / 3B 模型在手机 SoC 上的吞吐极限推演
   - 7.3 端侧投机采样 (Speculative Decoding: Small Draft Model on HTP + Target Model)
   - 7.4 分块预填 (Chunked Prefill) 与实时防掉帧调度
7. [模块六：生产环境极端精度与性能故障排查案例 (Hard RCA Scenarios)](#7-模块六生产环境极端精度与性能故障排查案例-hard-rca-scenarios)
   - 7.1 案例一：模型 FP32 正常，INT8 量化在 HTP 上推理输出 NaN 或精度断崖式下跌
   - 7.2 案例二：端侧 LLM 首字延迟 (TTFT) 达标，但随着上下文增长吞吐量 (TPS) 出现断崖式暴跌
   - 7.3 案例三：多模型并发运行在 HTP 时引发 VTCM Spilling 与严重卡顿
8. [Senior Staff AI 面试核心表达策略与 Checklist](#8-senior-staff-ai-面试核心表达策略与-checklist)

---

## 1. Senior Staff AI 岗位定位与技术图谱

在 Qualcomm，**Senior Staff AI Software Engineer** 负责主导将前沿深度学习模型（LLM、Vision-Language、Diffusion、Perception）高效部署至骁龙（Snapdragon）异构计算平台（Mobile、Compute、XR、Automotive）。

```
+----------------------------------------------------------------------------------------------------+
|                   Qualcomm Senior Staff AI Software Engineering Architecture Map                   |
+----------------------------------------------------------------------------------------------------+
| 1. High-Level Frameworks   | PyTorch, ONNX, ExecuTorch, LiteRT (TFLite), TorchDynamo               |
+----------------------------+-----------------------------------------------------------------------+
| 2. Optimization & Compres. | AIMET (CLE, Bias Correction, AdaRound, QAT), SmoothQuant, AWQ, INT4/8 |
+----------------------------+-----------------------------------------------------------------------+
| 3. Qualcomm AI Stack (QNN) | QNN Core C APIs (QnnGraph, QnnContext), Context Binary AOT, FastRPC   |
+----------------------------+-----------------------------------------------------------------------+
| 4. Hardware Acceleration   | Hexagon HTP (HMX Matrix + HVX 1024-bit Vector), VTCM SRAM, Adreno GPU  |
+----------------------------+-----------------------------------------------------------------------+
| 5. On-Device Generative AI | PagedAttention, KV Cache Quantization, Chunked Prefill, Speculative   |
+----------------------------+-----------------------------------------------------------------------+
| 6. Debugging & Profiling   | Per-layer Cosine/SNR Triage, Snapdragon Profiler, VTCM Spilling Trace  |
+----------------------------------------------------------------------------------------------------+
```

---

## 2. 模块一：高阶 C++ / AI 系统级 Coding 真题与源码

### 2.1 题目一：INT8 对称/非对称定点量化与反量化算子 (Quantize / Dequantize Kernel)
* **考察点：** 浮点到定点的饱和截断（Saturation Clamp）、Scale 与 ZeroPoint 的动态计算、防溢出与舍入误差控制。

```cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

struct QuantParams {
  float scale;
  int32_t zero_point;
};

// 计算非对称量化参数 (Asymmetric Quantization: Maps [min_val, max_val] -> [-128, 127])
QuantParams CalculateQuantParams(float min_val, float max_val, int qmin = -128, int qmax = 127) {
  // 保证 0.0f 能够被精确表示
  min_val = std::min(min_val, 0.0f);
  max_val = std::max(max_val, 0.0f);

  if (min_val == max_val) {
    return {1.0f, 0};
  }

  float scale = (max_val - min_val) / static_cast<float>(qmax - qmin);
  float initial_zero_point = static_cast<float>(qmin) - min_val / scale;

  int32_t zero_point = 0;
  if (initial_zero_point < qmin) {
    zero_point = qmin;
  } else if (initial_zero_point > qmax) {
    zero_point = qmax;
  } else {
    zero_point = static_cast<int32_t>(std::round(initial_zero_point));
  }

  return {scale, zero_point};
}

// 向量化量化执行算子
void QuantizeTensor(const float* input, int8_t* output, size_t size, const QuantParams& params) {
  const float inv_scale = 1.0f / params.scale;
  for (size_t i = 0; i < size; ++i) {
    // x_q = round(x / scale) + zero_point
    int32_t q_val = static_cast<int32_t>(std::round(input[i] * inv_scale)) + params.zero_point;
    // 饱和截断 (Saturate Clamp)
    q_val = std::max(-128, std::min(127, q_val));
    output[i] = static_cast<int8_t>(q_val);
  }
}

// 反量化执行算子
void DequantizeTensor(const int8_t* input, float* output, size_t size, const QuantParams& params) {
  for (size_t i = 0; i < size; ++i) {
    // x = (x_q - zero_point) * scale
    output[i] = (static_cast<float>(input[i]) - params.zero_point) * params.scale;
  }
}
```

---

### 2.2 题目二：端侧 LLM 分页 KV Cache 内存管理器 (Paged KV Cache Manager)
* **场景：** 类似于 vLLM / PagedAttention，在端侧有限内存中按固定 Block/Page 分配 KV 缓存，杜绝内存碎片并支持动态增长。

```cpp
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

constexpr size_t PAGE_SIZE = 16; // 每个物理 Page 容纳 16 个 Token 的 KV 向量

struct PhysicalBlock {
  int32_t block_id;
  std::vector<float> k_data; // 大小 = PAGE_SIZE * num_heads * head_dim
  std::vector<float> v_data;
};

class PagedKVCacheManager {
 public:
  PagedKVCacheManager(size_t total_blocks, size_t head_dim, size_t num_heads)
      : head_dim_(head_dim), num_heads_(num_heads) {
    for (size_t i = 0; i < total_blocks; ++i) {
      free_blocks_.push_back(static_cast<int32_t>(i));
      PhysicalBlock block;
      block.block_id = static_cast<int32_t>(i);
      block.k_data.resize(PAGE_SIZE * num_heads * head_dim, 0.0f);
      block.v_data.resize(PAGE_SIZE * num_heads * head_dim, 0.0f);
      block_pool_.push_back(std::move(block));
    }
  }

  // 为特定请求分配 Token 槽位
  bool AllocateTokenSlot(uint64_t request_id, int32_t& out_block_id, size_t& out_offset_in_block) {
    auto& table = request_page_tables_[request_id];
    size_t current_tokens = request_token_counts_[request_id];

    if (current_tokens % PAGE_SIZE == 0) {
      // 当前 Page 已满，需要申请新物理块
      if (free_blocks_.empty()) {
        return false; // Out of Memory (端侧内存耗尽)
      }
      int32_t new_block_id = free_blocks_.back();
      free_blocks_.pop_back();
      table.push_back(new_block_id);
    }

    out_block_id = table.back();
    out_offset_in_block = current_tokens % PAGE_SIZE;
    request_token_counts_[request_id]++;
    return true;
  }

  // 请求结束，回收所有占用的物理 Page
  void FreeRequest(uint64_t request_id) {
    if (request_page_tables_.find(request_id) != request_page_tables_.end()) {
      for (int32_t block_id : request_page_tables_[request_id]) {
        free_blocks_.push_back(block_id);
      }
      request_page_tables_.erase(request_id);
      request_token_counts_.erase(request_id);
    }
  }

 private:
  size_t head_dim_;
  size_t num_heads_;
  std::vector<int32_t> free_blocks_;
  std::vector<PhysicalBlock> block_pool_;
  std::unordered_map<uint64_t, std::vector<int32_t>> request_page_tables_; // Logical -> Physical Map
  std::unordered_map<uint64_t, size_t> request_token_counts_;
};
```

---

### 2.3 题目三：VTCM / SRAM 片上内存分块矩阵乘法 (Tiled Matrix Multiplication with Scratchpad Buffer)
* **场景：** Hexagon HTP 拥有 ~8MB 的片上紧耦合内存（VTCM）。大矩阵乘法 $C = A \times B$ 无法全量放入 SRAM，必须按 $\mathrm{Mtile} \times \mathrm{Ktile}$ 与 $\mathrm{Ktile} \times \mathrm{Ntile}$ 分块搬运至片上计算。

```cpp
#include <algorithm>
#include <vector>

constexpr int TILE_M = 32;
constexpr int TILE_N = 32;
constexpr int TILE_K = 32;

// 模拟利用 VTCM 双缓冲的 Tiled GEMM: C = A * B
// A: M x K, B: K x N, C: M x N
void TiledGEMM_VTCM(const float* A, const float* B, float* C, int M, int N, int K) {
  // 片上 Scratchpad Buffer (模拟分配在 VTCM 的静态数组)
  float tile_A[TILE_M][TILE_K];
  float tile_B[TILE_K][TILE_N];
  float tile_C[TILE_M][TILE_N];

  for (int m = 0; m < M; m += TILE_M) {
    int actual_m = std::min(TILE_M, M - m);

    for (int n = 0; n < N; n += TILE_N) {
      int actual_n = std::min(TILE_N, N - n);

      // 初始化片上累加器
      for (int i = 0; i < actual_m; ++i) {
        for (int j = 0; j < actual_n; ++j) {
          tile_C[i][j] = 0.0f;
        }
      }

      for (int k = 0; k < K; k += TILE_K) {
        int actual_k = std::min(TILE_K, K - k);

        // 模拟 DMA 从 DDR 搬运 A_tile 到 VTCM
        for (int i = 0; i < actual_m; ++i) {
          for (int p = 0; p < actual_k; ++p) {
            tile_A[i][p] = A[(m + i) * K + (k + p)];
          }
        }

        // 模拟 DMA 从 DDR 搬运 B_tile 到 VTCM
        for (int p = 0; p < actual_k; ++p) {
          for (int j = 0; j < actual_n; ++j) {
            tile_B[p][j] = B[(k + p) * N + (n + j)];
          }
        }

        // 片上加速核心 (HMX / HVX) 乘累加计算
        for (int i = 0; i < actual_m; ++i) {
          for (int p = 0; p < actual_k; ++p) {
            float a_val = tile_A[i][p];
            for (int j = 0; j < actual_n; ++j) {
              tile_C[i][j] += a_val * tile_B[p][j];
            }
          }
        }
      }

      // 将片上结果写回主存 C
      for (int i = 0; i < actual_m; ++i) {
        for (int j = 0; j < actual_n; ++j) {
          C[(m + i) * N + (n + j)] = tile_C[i][j];
        }
      }
    }
  }
}
```

---

### 2.4 题目四：端侧生成式 Top-K 与 Top-P (Nucleus) 采样算法
* **场景：** 在手机端执行 LLM 自回归生成时，需对 Logits 进行 Temperature 缩放、Softmax 归一化，并结合 Top-K 与 Top-P 进行高效随机采样。

```cpp
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

struct TokenProb {
  int id;
  float prob;
};

int SampleTopKTopP(const float* logits, int vocab_size, float temperature, int top_k, float top_p) {
  std::vector<TokenProb> candidates(vocab_size);
  float max_logit = -1e9f;
  for (int i = 0; i < vocab_size; ++i) {
    max_logit = std::max(max_logit, logits[i]);
  }

  // 1. Softmax with Temperature Scaling (减去 max_logit 防止指数溢出)
  float sum_exp = 0.0f;
  for (int i = 0; i < vocab_size; ++i) {
    candidates[i].id = i;
    float scaled = (logits[i] - max_logit) / temperature;
    candidates[i].prob = std::exp(scaled);
    sum_exp += candidates[i].prob;
  }
  for (int i = 0; i < vocab_size; ++i) {
    candidates[i].prob /= sum_exp;
  }

  // 2. Top-K 截断 (Partial Sort 到前 K 个)
  int k = std::min(top_k, vocab_size);
  std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end(),
                    [](const TokenProb& a, const TokenProb& b) { return a.prob > b.prob; });
  candidates.resize(k);

  // 3. Top-P (Nucleus) 累积概率截断
  float cum_prob = 0.0f;
  int p_cutoff = 0;
  for (int i = 0; i < k; ++i) {
    cum_prob += candidates[i].prob;
    p_cutoff = i + 1;
    if (cum_prob >= top_p) break;
  }
  candidates.resize(p_cutoff);

  // 4. 重新归一化并抽样
  float filtered_sum = 0.0f;
  for (const auto& c : candidates) filtered_sum += c.prob;

  static std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<float> dist(0.0f, filtered_sum);
  float random_val = dist(gen);

  float current_sum = 0.0f;
  for (const auto& c : candidates) {
    current_sum += c.prob;
    if (random_val <= current_sum) {
      return c.id;
    }
  }

  return candidates.back().id;
}
```

---

## 3. 模块二：Qualcomm AI Stack 软件架构与 QNN SDK 深度剖析

### 3.1 QNN vs. SNPE 演进与统一 C API 架构
* **SNPE (Legacy):** 高度单体化（Monolithic），强依赖专有的 `.dlc` 文件格式，难以支持动态图与细粒度内存定制。
* **QNN (Qualcomm Neural Network SDK / QAIRT):**
  * **统一 C API 接口 (`QnnInterface_t`)：** 所有后端（HTP、GPU、CPU、LPAI）实现相同的抽象接口。
  * **显式图构建 (`QnnGraph`)：** 支持 AOT（Ahead-Of-Time）和 JIT（Just-In-Time）图构建。
  * **显式内存注册 (`QnnMem_t`)：** 允许应用程序直接传入 Linux `dma-buf` 或 ION Buffer 句柄，实现零拷贝直通 NPU。

---

### 3.2 端到端模型部署工作流 (PyTorch $\rightarrow$ Hexagon HTP)

```
[ PyTorch / ONNX Model ]
           │
           ▼
[ AIMET Quantization ] ──> Generates Encodings (Scale & Offset JSON)
           │
           ▼
[ qnn-onnx-converter ] ──> Generates model.cpp + model.bin (QNN IR)
           │
           ▼
[ qnn-model-lib-generator ] ──> Compiles libmodel.so
           │
           ▼
[ qnn-context-binary-generator ] ──> (HTP Optimizer: Kernel Selection, Op Fusion, VTCM Layout)
           │
           ▼
[ Compiled Context Binary (*.serialized.bin) ] ──> (Deploys to Device via QNN Runtime)
```

---

## 4. 模块三：Hexagon NPU / HTP 硬件架构与底层加速原理

### 4.1 HTP 架构核心拓扑 (HMX + HVX + VTCM)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Hexagon Tensor Processor (HTP)                     │
│                                                                             │
│  ┌─────────────────────────┐             ┌───────────────────────────────┐  │
│  │   HMX (Matrix Engine)   │             │      HVX (Vector Engine)      │  │
│  │   • INT8/INT4/FP16 GEMM │             │      • 1024-bit SIMD Vector   │  │
│  │   • Tensor Convolution  │             │      • Non-linear Activations │  │
│  └────────────┬────────────┘             └───────────────┬───────────────┘  │
│               │                                          │                  │
│               ▼                                          ▼                  │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                VTCM (Vector Tightly-Coupled Memory)                   │  │
│  │                • Ultra-High Bandwidth On-Chip SRAM (~8MB)             │  │
│  │                • Hardware DMA Double-Buffering Controller             │  │
│  └───────────────────────────────────┬───────────────────────────────────┘  │
└──────────────────────────────────────┼──────────────────────────────────────┘
                                       │ AXI Bus (DMA)
                                       ▼
                             [ System DDR Memory ]
```

* **HMX (Hexagon Matrix Extensions):** 负责计算密集的 GEMM 与卷积，支持 INT8 激活 $\times$ INT4 权重的混合精度计算。
* **HVX (Hexagon Vector Extensions):** 1024-bit 宽向量单元，负责非线性激活函数（GELU、SiLU、Softmax）、LayerNorm / RMSNorm 及 Element-wise 算子。
* **VTCM (Vector Tightly-Coupled Memory):** 片上超高带宽 SRAM。HTP 编译器会自动调度 DMA 双缓冲，使得“当前分块的计算”与“下一分块的数据搬运”在时间线上完全重叠（Ping-Pong Overlapping）。

---

## 5. 模块四：模型量化、压缩与 AIMET 工具链体系

### 5.1 CLE (Cross-Layer Equalization) 与 Bias Correction
* **CLE 原理：** 许多卷积网络（如 MobileNetV2 / ResNet）某些通道权重极小而某些通道极大，导致 Per-Tensor 量化时小通道精度严重丢失。CLE 利用 ReLU/PReLU 的正齐次性（Positive Homogeneity $f(s \cdot x) = s \cdot f(x)$），通过权重缩放矩阵 $S$ 平衡相邻两层卷积的权重范围：
  $$
\mathrm{W1}' = S^{-1} \mathrm{W1}, \quad \mathrm{W2}' = \mathrm{W2} S
$$
* **Bias Correction：** 补偿量化带来的均值漂移（Quantization Shift Error），通过分析校准集输出误差直接修正 Bias 偏置。

---

### 5.2 AdaRound (Adaptive Rounding)
* 传统的最近舍入（Round-to-Nearest）并不是最小化任务损失的最优解。AdaRound 通过在无标注校准集上优化二值松弛目标函数，学习每一项权重的最佳向上/向下舍入方向：
  $$
\underset{V}{\min} \| W x - \tilde{W}(V) x \|F^2 + \lambda \mathrm{freg}(V)
$$

---

## 6. 模块五：端侧大模型 (On-Device LLM & GenAI) 部署与优化体系

### 6.1 Prefill 与 Decode 阶段的 Roofline 模型推演

```
Arithmetic Intensity (FLOPs / Byte) = Total FLOPs / Total Memory Access
```

* **Prefill 阶段（Prompt 编码）：**
  * 特征：一次性输入 $N$ 个 Prompt Tokens，矩阵乘为标准 GEMM（Compute-Bound）。
  * 瓶颈：HMX 算力上限（TOPS）。
* **Decode 阶段（逐 Token 自回归生成）：**
  * 特征：每次生成 1 个 Token，需将全部模型权重从内存读取一遍（GEMV, Memory-Bandwidth Bound）。
  * 算力强度极低（$\sim 1\text{ FLOP / Byte}$），瓶颈严格受限于手机 DDR 内存带宽。

### 6.2 7B INT4 模型在移动端 DRAM 上的吞吐极限计算
* **参数设定：**
  * 模型大小：7B INT4 权重 $\approx 3.5\text{ GB}$。
  * 手机 LPDDR5X 理论带宽：$64\text{ GB/s}$；实际可用带宽（考虑 CPU/GPU 争抢）：$\approx 35\text{ GB/s}$。
* **理论生成速率上限（Tokens Per Second / TPS）：**
  $$\text{Max TPS} = \frac{\text{Available Bandwidth}}{\text{Model Size}} = \frac{35\text{ GB/s}}{3.5\text{ GB/token}} \approx 10.0\text{ Tokens/s}$$
* **结论：** 若要突破 20+ TPS，必须采用 **INT4/INT3 混合量化、投机采样（Speculative Decoding）或更小的 2B/3B 蒸馏模型**。

---

## 7. 模块六：生产环境极端精度与性能故障排查案例 (Hard RCA)

### 7.1 案例一：模型 FP32 正常，INT8 量化在 HTP 上推理输出 NaN 或精度断崖式下跌
* **排查根因链路：**
  1. **层间信噪比（Per-Layer SQNR / Cosine Similarity）逐层探查：** 利用 QNN Profiler 配合 FP32 Golden 结果，比对每层输出的余弦相似度，定位出现精度断崖的首个算子（First Degradation Layer）。
  2. **激活值离群点（Outliers）检测：** 检查该层输入是否存在极端数值尖峰（如 LayerNorm 后出现值域达 $[-50, +50]$ 的点，导致 INT8 的 Scale 极大，其余正常值被全部量化为 0）。
  3. **解决方案：**
     * 采用 **Per-Channel 量化** 代替 Per-Tensor 量化。
     * 使用 AIMET 引入 **SmoothQuant / Act-Clip** 抑制尖峰。
     * 将敏感层（如 Softmax 前后的 Attention Matrix）保留为 **FP16 混合精度** 执行。

---

### 7.2 案例二：端侧 LLM 首字延迟达标，但随着上下文增长 TPS 出现断崖式暴跌
* **排查根因链路：**
  1. **KV Cache 未分页引起频繁内存重分配与碎片：** 传统连续数组在序列长度超过预分配大小时触发 `realloc`，导致主存剧烈抖动与 GC 阻塞。
  2. **KV Cache 溢出导致 DDR 带宽耗尽：** 检查是否对 KV Cache 进行了量化（FP8 / INT4）。未量化的 FP16 KV Cache 在 2048 长度下占用的带宽已超过权重搬运开销。
  3. **解决方案：** 引入 **PagedAttention** 固定页块管理，并开启 **INT8/FP8 KV Cache 压缩**。

---

## 8. Senior Staff AI 面试核心表达策略与 Checklist

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                   Senior Staff AI Interview Delivery Principles                   │
├───────────────────────────────────────────────────────────────────────────────────┤
│ 1. 软硬协同视角 (HW/SW Co-Design Mindset)                                         │
│    • 讨论模型优化时，必须联系到 NPU 底层：HMX 矩阵算力、HVX 向量、VTCM 双缓冲。     │
├───────────────────────────────────────────────────────────────────────────────────┤
│ 2. 算力与带宽平衡直觉 (Roofline & Memory Bounds)                                   │
│    • 熟练区分 Compute-Bound (Prefill) 与 Memory-Bound (Decode) 并给出公式量化计算。 │
├───────────────────────────────────────────────────────────────────────────────────┤
│ 3. 生产级精度调优方法论 (Systematic Quantization Triage)                            │
│    • 不说“凭感觉调参”，而是阐述：CLE -> AdaRound -> Per-Channel -> FP16 混合精度链路。 │
└───────────────────────────────────────────────────────────────────────────────────┘
```
