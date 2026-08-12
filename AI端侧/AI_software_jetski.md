# Qualcomm AI Software Engineer Complete Interview Guide & Solutions


> **GitHub 公式注意：** 站点数学引擎里 `_` 极易破坏渲染。下文公式使用无下划线命名（如 `Tideal`、`HKV`）；带下划线的工程名写在 `text` 代码块里。
---

## Role & Technical Scope Overview
An **AI Software Engineer** at Qualcomm develops, optimizes, and deploys deep learning models and runtime software across Qualcomm Snapdragon platforms (Mobile, Compute/Laptops, Automotive, IoT, and XR).

The core technical pillars evaluated in Qualcomm AI Software interviews include:
1. **Qualcomm AI Stack & Toolchains**: QNN (Qualcomm Neural Network SDK), SNPE, AIMET (AI Model Efficiency Toolkit), Qualcomm AI Hub, and ONNX Runtime QNN Execution Provider.
2. **Model Optimization & Quantization**: PTQ (CLE, Bias Correction, AdaRound), QAT, INT8/INT4/FP8/FP16 arithmetic, outlier handling (SmoothQuant, AWQ), and operator fusion.
3. **Hardware Architecture Acceleration (Hexagon NPU / HTP)**: Hexagon Vector Extensions (HVX), Hexagon Matrix Extensions (HMX), VTCM (Vector Tightly-Coupled Memory), DMA ping-pong buffering, Adreno GPU (OpenCL), and FastRPC.
4. **On-Device Generative AI & LLM Deployment**: KV caching, token generation memory bandwidth vs compute bound (Roofline model), PagedAttention, speculative decoding, and Diffusion on-device pipelines.
5. **Runtime Frameworks & Custom Kernels**: ExecuTorch, TorchDynamo, writing custom QNN Ops using C++/HVX intrinsics, and graph partitioners.
6. **Performance Profiling & Numerical Debugging**: Accuracy loss triage, per-layer SNR/Cosine similarity analysis, latency/throughput profiling with QNN profiler and Snapdragon Profiler.

---

# Part 1: Qualcomm AI Stack Architecture (QNN, SNPE, AIMET)

---

### Question 1: Qualcomm AI Stack Architecture & Execution Flow
> **Describe the Qualcomm AI Stack architecture. What is the difference between QNN and SNPE? How does a trained PyTorch/ONNX model transition from model definition to on-device execution on the Hexagon NPU?**

#### Solution:

#### 1. Architecture Hierarchy
```
+-----------------------------------------------------------------------------------+
|               Framework Layer: PyTorch / TensorFlow / ONNX / ExecuTorch           |
+-----------------------------------------------------------------------------------+
                                         |
+-----------------------------------------------------------------------------------+
|             AIMET (AI Model Efficiency Toolkit) - Quantization & Compression      |
|             (CLE, Bias Correction, AdaRound, QAT, Mixed Precision)                |
+-----------------------------------------------------------------------------------+
                                         | Quantized ONNX / TorchScript / QNN Model
+-----------------------------------------------------------------------------------+
|                           Qualcomm Neural Network (QNN) SDK                       |
|  +-----------------------------------------------------------------------------+  |
|  | QNN Core APIs: QnnContext, QnnGraph, QnnTensor, QnnOpConfig                 |  |
|  +-----------------------------------------------------------------------------+  |
|  | QNN Converters & Model Compiler (qnn-onnx-converter, qnn-model-lib-generator)|  |
|  +-----------------------------------------------------------------------------+  |
|  | Execution Backends:                                                         |  |
|  |  [ QNN HTP (NPU) ]   [ QNN GPU (OpenCL) ]   [ QNN CPU ]   [ QNN DSP (Legacy) ] |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
                                         | FastRPC / Drivers
+-----------------------------------------------------------------------------------+
|                        Hardware Layer (Hexagon HTP / Adreno GPU)                  |
+-----------------------------------------------------------------------------------+
```

#### 2. QNN vs. SNPE
* **SNPE (Snapdragon Neural Processing Engine)**: Qualcomm's legacy inference engine. Relies on converting models into a proprietary `.dlc` (Deep Learning Container) binary format. It is higher-level and monolithic.
* **QNN (Qualcomm Neural Network SDK)**: Modern, unified, modular API architecture. 
  * Provides a unified C API (`QnnInterface_t`) allowing multiple backend targets (HTP, GPU, CPU).
  * Exposes explicit graph construction, backend extensions, custom op registration, and zero-copy memory registrations via standard memory handles (`QnnMem_t`).
  * Used internally by ONNX Runtime (`QNN Execution Provider`) and ExecuTorch.

#### 3. End-to-End Deployment Workflow (PyTorch $\rightarrow$ Hexagon HTP)
1. **Model Export**: PyTorch model $\rightarrow$ `torch.onnx.export()` with fixed or dynamic shapes.
2. **AIMET Quantization**: Run Post-Training Quantization (PTQ) to produce quantization encodings (scale $S$ and offset $Z$ per tensor).
3. **QNN Model Conversion (`qnn-onnx-converter`)**:
   * Parses ONNX graph and AIMET encodings $\rightarrow$ generates C++ model file (`model.cpp`) and binary weights (`model.bin`) representing the QNN Graph.
4. **Graph Compilation (`qnn-model-lib-generator`)**:
   * Compiles `model.cpp` into a shared library (`libmodel.so`) containing graph structure and serialized op parameters.
5. **Context Binary Generation (Offline Preparation via `qnn-context-binary-generator`)**:
   * HTP backend optimizes the graph (kernel selection, op fusion, VTCM memory allocation, weight transformation) $\rightarrow$ outputs a serialized `.bin` context binary.
6. **On-Device Runtime Execution**:
   * App initializes `QnnBackend` (HTP) $\rightarrow$ creates `QnnContext` from context binary $\rightarrow$ registers I/O buffers (`QnnMem_register`) via RPC memory $\rightarrow$ calls `QnnGraph_execute()`.

---

### Question 2: Graph Optimization & Operator Fusion in QNN
> **Explain the common graph optimizations performed by QNN and how operator fusion improves execution efficiency and memory bandwidth on mobile NPUs.**

#### Solution:

#### 1. Why Operator Fusion is Critical on Edge NPUs
Mobile inference is predominantly **memory-bandwidth limited** rather than compute-limited for many layers. 
* Unfused: `Conv2D` writes output tensor to DDR RAM $\rightarrow$ `BatchNorm` reads from DDR, computes, writes back $\rightarrow$ `ReLU` reads from DDR, computes, writes back. Memory traffic: $3 \times \text{writes} + 2 \times \text{reads}$.
* Fused: `Conv2D + BN + ReLU` executes in a single pass in Hexagon local VTCM/registers without writing intermediate activations to external DDR.

#### 2. Key Operator Fusion Patterns in QNN
1. **Linear Layer + Activation Fusion**:
   $$\text{Conv2D} + \text{BatchNorm} + \text{ReLU/SiLU/GELU} \longrightarrow \text{FusedConv2D}$$
   * *Math for Conv + BN folding*:
     $$
\mathrm{Wfused} = \frac{\gamma}{\sqrt{\sigma^2 + \epsilon}} \cdot W, \quad \mathrm{bfused} = \frac{\gamma}{\sqrt{\sigma^2 + \epsilon}} \cdot (b - \mu) + \beta
$$
2. **MatMul + Bias + Add (Residual) Fusion**:
   $$\text{MatMul}(X, W) + \text{Bias} + \text{Residual} \longrightarrow \text{FusedMatMulAdd}$$
   * Avoids allocating an intermediate accumulator buffer for transformer blocks.
3. **LayerNorm / RMSNorm Fusion**:
   * Fuses variance computation, normalization, scale, and shift into a single hardware HVX vector pipeline loop.
4. **Attention Fusion**:
   * Fuses $Q \times K^T \rightarrow \text{Scale} \rightarrow \text{Softmax} \rightarrow \times V$ into a unified FlashAttention-style micro-kernel to minimize $S \times S$ score matrix materialization in memory.

---

# Part 2: Model Compression & Quantization (AIMET, PTQ, QAT)

---

### Question 3: Quantization Fundamentals (Symmetric vs. Asymmetric, Formulas)
> **Explain the mathematical formulation of 8-bit uniform quantization. What is the difference between Symmetric and Asymmetric quantization? When would you use Per-Tensor vs. Per-Channel vs. Per-Group quantization?**

#### Solution:

#### 1. Mathematical Formulation
Uniform affine quantization maps a continuous floating-point value $x \in [\alpha, \beta]$ to an integer $q \in [\mathrm{q}, \mathrm{q}]$ (e.g., $[0, 255]$ for `uint8` or $[-128, 127]$ for `int8`):

$$
\text{Quantize}(x) = q = \text{clamp}\left( \left\lfloor \frac{x}{S} \right\rceil + Z, \; \mathrm{q}, \; \mathrm{q} \right)
$$
$$\text{Dequantize}(q) = \hat{x} = S \cdot (q - Z)$$

Where:
* $S$ is the **Scale Factor** (positive real number): $S = \frac{\beta - \alpha}{\mathrm{q} - \mathrm{q}}$
* $Z$ is the **Zero Point / Offset** (integer matching the quantized data type): $Z = \text{round}\left( \frac{-\alpha}{S} \right) + \mathrm{q}$

#### 2. Symmetric vs. Asymmetric Quantization
| Property | Symmetric Quantization | Asymmetric Quantization |
| :--- | :--- | :--- |
| **Zero Point ($Z$)** | Constrained to **$Z = 0$** (Floating point $0.0$ maps exactly to integer $0$). | Arbitrary integer $Z \ne 0$. Floating point $0.0$ maps to non-zero integer. |
| **Clipping Range** | Symmetric around zero: $[-\max(|\alpha|, |\beta|), +\max(|\alpha|, |\beta|)]$. | Asymmetric $[\alpha, \beta]$. |
| **Computational Overhead** | Highly efficient: $X \cdot W = \mathrm{SX} \mathrm{SW} \sum (\mathrm{qX} \cdot \mathrm{qW})$. No zero-point cross terms. | Slower: Requires compensating for $\mathrm{ZX}$ and $\mathrm{ZW}$: $\sum (\mathrm{qX} - \mathrm{ZX})(\mathrm{qW} - \mathrm{ZW})$. |
| **Best Used For** | Weights (which are typically centered around zero) and symmetric activations (tanh). | Unbounded/unilateral activations like ReLU/GELU ($x \ge 0$). |

#### 3. Granularity: Per-Tensor vs. Per-Channel vs. Per-Group
* **Per-Tensor**: Single $S$ and $Z$ for the entire tensor. Lowest memory overhead; high risk of precision loss if channels have different dynamic ranges.
* **Per-Channel (Per-Axis)**: Dedicated $\mathrm{Sc}$ and $\mathrm{Zc}$ for each output channel of a convolution or linear weight matrix. **Standard for weights in QNN/HTP**, preventing one dominant channel from destroying the precision of smaller channels.
* **Per-Group / Block-wise (e.g., Group Size = 64/128)**: Sub-divides a channel into small blocks with individual scale factors. **Standard for 4-bit LLM weight quantization (W4A16 / W4A8)** to preserve outlier distributions.

---

### Question 4: AIMET Post-Training Quantization (CLE, Bias Correction, AdaRound)
> **What techniques does Qualcomm AIMET provide to fix quantization accuracy drop without full retraining? Explain Cross-Layer Equalization (CLE), Bias Correction, and AdaRound.**

#### Solution:

```
[ FP32 Model ]
      |
      v
+-------------------------------+
|  Cross-Layer Equalization     |  ==> Balances weight ranges between consecutive
|  (Scale factor r exchange)    |      layers using ReLU homogeneity.
+-------------------------------+
      |
      v
+-------------------------------+
|  Bias Correction              |  ==> Corrects output statistical mean shift
|  (E[y] - E[q(y)])             |      introduced by asymmetric clipping.
+-------------------------------+
      |
      v
+-------------------------------+
|  AdaRound (Adaptive Rounding) |  ==> Solves layer-wise MSE optimization to decide
|  (Up vs. Down rounding)       |      whether to round up or down per weight.
+-------------------------------+
      |
      v
[ High-Accuracy Quantized Model (INT8/INT4) ]
```

#### 1. Cross-Layer Equalization (CLE)
* **Problem**: In consecutive Conv/Linear layers ($y = \mathrm{W2} \cdot \text{ReLU}(\mathrm{W1} \cdot x)$), certain channels in $\mathrm{W1}$ have very large dynamic ranges, while others are tiny. Quantizing $\mathrm{W1}$ per-tensor or per-channel leaves fine channels with near-zero resolution.
* **Solution**: Exploit the positive scaling property of ReLU ($\text{ReLU}(r \cdot z) = r \cdot \text{ReLU}(z)$ for $r > 0$).
  * Scale down the heavy channel in $\mathrm{W1}$ by factor $\mathrm{ri}$: $\mathrm{W1}'(i, :) = \mathrm{W1}(i, :) / \mathrm{ri}$.
  * Scale up the corresponding input channel in $\mathrm{W2}$ by factor $\mathrm{ri}$: $\mathrm{W2}(:, i) = \mathrm{W2}(:, i) \cdot \mathrm{ri}$.
  * Mathematically exact equivalence in FP32; equalizes dynamic ranges across layers before quantizing.

#### 2. Empirical Bias Correction
* **Problem**: Quantization error is not strictly zero-mean, causing systematic drift in the expected output: $E[\hat{y}] \ne E[y]$.
* **Solution**: Measure the mean error on a calibration dataset: $\Delta \mu = E[W x + b] - E[\hat{W} \hat{x} + b]$.
* Adjust the layer's bias term: $\mathrm{bcorrected} = b - \Delta \mu$.

#### 3. AdaRound (Adaptive Rounding)
* **Problem**: Standard rounding ($\text{round}(w/S) = \lfloor w/S + 0.5 \rfloor$) rounds to the nearest integer, which is sub-optimal for task loss.
* **Solution**: Formulates rounding as a quadratic optimization problem minimizing layer output reconstruction error:
  $$
\underset{V}{\min} \| W x - \hat{W}(V) x \|F^2 + \lambda \cdot R(V)
$$
  Where $V$ is a continuous parameter between 0 and 1 deciding whether to round up ($\lceil w/S \rceil$) or down ($\lfloor w/S \rfloor$). Uses a small unlabelled dataset (~100–500 batches) and completes in minutes.

---

### Question 5: Handling Activation Outliers in LLMs (SmoothQuant & AWQ)
> **Why do Large Language Models (LLMs) suffer severe accuracy drops under INT8/INT4 activation quantization, and how do SmoothQuant and AWQ solve this problem on Qualcomm hardware?**

#### Solution:

#### 1. The Activation Outlier Problem
In transformer models (LLaMA, Mistral, GPT), activations develop systematic, high-magnitude outlier features across specific hidden dimensions (e.g., $100\times$ larger than median values).
* Quantizing activations to INT8 truncates normal features (high clipping error) or spreads the 256 quantization bins over the outlier range (destroying resolution for 99% of tokens).
* However, **weights are easy to quantize**, while **activations are hard**.

#### 2. SmoothQuant: Migration of Quantization Difficulty
SmoothQuant applies a per-channel scaling factor $s \in \mathbb{R}^C$ to mathematically transfer the dynamic range difficulty from activations $X$ to weights $W$:

$$Y = X \cdot W = (X \cdot \text{diag}(s)^{-1}) \cdot (\text{diag}(s) \cdot W) = \hat{X} \cdot \hat{W}$$

* Scale Factor Calculation:
  $$
\mathrm{sj} = \frac{\max(|\mathrm{Xj}|)^\alpha}{\max(|\mathrm{Wj}|)^{1 - \alpha}}
$$
  Where $\alpha \in [0, 1]$ is a migration strength hyperparameter (typically $\alpha = 0.5$).
* Activations are divided by $\mathrm{sj}$ (suppressing outliers), and weights are multiplied by $\mathrm{sj}$ (absorbed into weight matrices offline). Both $\hat{X}$ and $\hat{W}$ can now be quantized with standard INT8 arithmetic on Qualcomm HTP.

#### 3. AWQ (Activation-aware Weight Quantization) for 4-bit Weights
* Protects the top 1% salient weight channels corresponding to large activation magnitudes by keeping them at higher precision or applying per-channel protection scales, enabling W4A16 / W4A8 execution with negligible perplexity degradation.

---

# Part 3: Hexagon NPU / HTP Architecture & Hardware Acceleration

---

### Question 6: Hexagon HTP Architecture (HVX vs. HMX, VTCM, Memory Hierarchy)
> **Describe the internal architecture of Qualcomm's Hexagon Tensor Processor (HTP). What are HVX, HMX, and VTCM? How do you write software to maximize throughput on HTP?**

#### Solution:

```
+-------------------------------------------------------------------------------+
|                            Qualcomm Hexagon Core (HTP)                        |
|                                                                               |
|  +---------------------------+             +-------------------------------+  |
|  |   HVX (Vector Extension)  |             |     HMX (Matrix Engine)       |  |
|  |  - 1024-bit / 128-byte     |             |  - 2D Systolic Matrix Mult    |  |
|  |    SIMD vector registers   |             |  - INT8 / INT4 / FP16 dot-prod|  |
|  |  - Element-wise, Activations|             |  - Peak TOPS for Conv & GEMM  |  |
|  |    Softmax, Normalization |             |                               |  |
|  +---------------------------+             +-------------------------------+  |
|               ^                                             ^                 |
|               |                                             |                 |
|  +-------------------------------------------------------------------------+  |
|  |          VTCM (Vector Tightly-Coupled Memory: 4MB - 8MB SRAM)           |  |
|  |          - Multi-terabyte/sec ultra-low latency internal scratchpad      |  |
|  +-------------------------------------------------------------------------+  |
|               ^                                                               |
|               | Async DMA Engine (Background prefetch / double-buffering)    |
|               v                                                               |
|  +-------------------------------------------------------------------------+  |
|  |                    DDR RAM (System Memory via FastRPC)                  |  |
+-------------------------------------------------------------------------------+
```

#### 1. Core Architectural Components
* **HMX (Hexagon Matrix Extensions)**: Dedicated 2D matrix multiplication accelerator (systolic array). Delivers the primary TOPS for `Conv2D` and `GEMM/MatMul`. Directly operates on INT8/INT4/FP16 tensors.
* **HVX (Hexagon Vector Extensions)**: 1024-bit (128-byte) SIMD vector execution units. Handles non-GEMM math: element-wise ops, activations (GELU, SiLU), reductions, Softmax, LayerNorm, and custom tensor manipulations.
* **VTCM (Vector Tightly-Coupled Memory)**: High-speed on-chip SRAM (4MB to 8MB+ depending on Snapdragon tier) physically adjacent to vector/matrix execution units with $>1\text{ TB/s}$ bandwidth.
* **Asynchronous DMA Engine**: Transfers data between system DDR RAM and VTCM in the background without stalling execution units.

#### 2. Principles for Maximizing HTP Throughput:
1. **DMA Pipelining / Ping-Pong (Double) Buffering**:
   * While HMX/HVX is computing on Buffer A in VTCM, DMA engine simultaneously loads the next tile into Buffer B from DDR and writes back previous results from Buffer C.
2. **Tiling & Memory Residency**:
   * Keep intermediate feature maps entirely resident in VTCM across fused subgraphs to eliminate DDR read/write round-trips.
3. **128-byte Vector Alignment**:
   * Align all memory pointers and tensor strides to 128-byte boundaries to avoid unaligned load penalties in HVX.

---

### Question 7: FastRPC & Zero-Copy Memory Management
> **How does an Android user-space application pass tensor buffers to the Hexagon DSP/NPU via FastRPC? How do you achieve zero-copy inference?**

#### Solution:

#### 1. FastRPC Mechanism
FastRPC is Qualcomm's high-performance IPC framework allowing user-space Linux/Android processes to invoke C/C++ functions executing on the Hexagon DSP/NPU (cDSP) real-time operating system (QuRT).
* Manages CPU cache maintenance, interrupt signaling, and remote thread scheduling.

#### 2. Achieving Zero-Copy Buffer Passing
* **Problem**: Standard RPC copies data across memory boundaries, incurring massive latency and power overhead for multi-megabyte image or activation tensors.
* **Zero-Copy Solution (ION / DMA-BUF / RPCMEM)**:
  1. **Allocation**: Allocate shared memory on the host CPU using `rpcmem_alloc()` or Android Gralloc/DMA-BUF (`O_CLOEXEC | DMA_BUF`).
  2. **Memory Registration**: Obtain the file descriptor (`dma_buf_fd`) and virtual address. Register the memory with QNN using `QnnMem_register()`.
  3. **FastRPC Mapping**: FastRPC maps the physical memory pages directly into the Hexagon DSP's SMMU (System Memory Management Unit) page tables.
  4. **Execution**: Pass the registered `Qnn_Tensor_t` containing the `QnnMem_t` handle. The HTP accesses physical RAM directly via DMA without any intermediate memory copy.
  5. **Cache Coherency**: Flush CPU cache before dispatch (`DMA_BUF_IOCTL_SYNC` with `DMA_BUF_SYNC_START/WRITE`) and invalidate after execution.

---

# Part 4: On-Device Generative AI & Large Language Model (LLM) Optimization

---

### Question 8: LLM Memory Bandwidth vs. Compute Bound (Prefill vs. Decode)
> **Explain why LLM token generation (Decode Phase) is memory-bandwidth bound while the Prompt processing (Prefill Phase) is compute-bound. How do you analyze this using the Roofline model?**

#### Solution:

#### 1. The Two Phases of LLM Inference
```
Prefill Phase (Prompt Evaluation):
  Input: N tokens (e.g., N = 512).
  Operation: GEMM (Matrix-Matrix Multiplication: [Batch, N, D] x [D, D]).
  Arithmetic Intensity: HIGH (~ O(N) operations per weight byte).
  Bottleneck: COMPUTE BOUND (HMX/NPU FLOPS limit).

Decode Phase (Token-by-Token Generation):
  Input: Exactly 1 new token at each step.
  Operation: GEMV (Matrix-Vector Multiplication: [Batch, 1, D] x [D, D]).
  Arithmetic Intensity: LOW (~ 1 FLOP per weight byte read from memory).
  Bottleneck: MEMORY BANDWIDTH BOUND (DDR RAM throughput limit).
```

#### 2. Roofline Model Formulation
$$\text{Attainable Performance (TOPS)} = \min\left( \text{Peak Hardware Compute}, \; \text{Operational Intensity} \times \text{Memory Bandwidth} \right)$$
$$\text{Operational Intensity} = \frac{\text{Total Operations (FLOPs)}}{\text{Total Memory Access (Bytes)}}$$

* **Calculation for Decode Step (e.g., 7B Model, INT4 Weights = 3.5 GB)**:
  * To generate **1 token**, all 3.5 billion weights ($3.5\text{ GB}$) must be fetched from DDR RAM.
  * Compute operations: $\approx 2 \times 7\times 10^9 = 14\text{ GFLOPs}$.
  * Operational Intensity: $14\text{ GFLOPs} / 3.5\text{ GB} = 4\text{ FLOPs/Byte}$.
  * If Snapdragon DDR bandwidth is $68\text{ GB/s}$:
    $$\text{Max Token Rate} = \frac{68\text{ GB/s}}{3.5\text{ GB/token}} \approx 19.4\text{ tokens/second}$$
* *Key Takeaway*: Compute capacity is sitting mostly idle during decode; the sole factor determining generation speed is weight size and memory bandwidth. This is why **4-bit weight quantization (W4A16)** is universally applied.

---

### Question 9: KV Cache Management, PagedAttention & Speculative Decoding on NPU
> **How does the KV cache grow during autoregressive generation? How do PagedAttention, quantized KV cache, and Speculative Decoding improve throughput on mobile NPUs?**

#### Solution:

#### 1. KV Cache Growth & Memory Bottleneck
In attention computation: $\text{Attention}(Q, K, V) = \text{Softmax}\left(\frac{Q K^T}{\sqrt{\mathrm{dk}}}\right) V$
* For each newly generated token, past key and value vectors must be preserved to compute self-attention.
* **Memory footprint for KV Cache**:
  $$
\mathrm{MemoryKV} = 2 \times B \times L \times H \times S \times D \times \text{BytesPerElement}
$$
  *(Batch $B$, Layers $L$, Heads $H$, Sequence Length $S$, Head Dim $D$).*
* For a 7B model at $S = 4096$ in FP16: $\text{Memory} \approx 2 \times 1 \times 32 \times 32 \times 4096 \times 128 \times 2\text{ bytes} \approx 2\text{ GB}$.

#### 2. Optimizations for Edge NPU Deployment:
1. **Quantized KV Cache (INT8 / FP8 / INT4)**:
   * Compresses stored $K, V$ activations from FP16 (2 bytes) to INT8 (1 byte) or INT4 (0.5 byte) with dynamic scale factors, cutting KV cache bandwidth and DDR footprint by $50\% - 75\%$.
2. **PagedAttention (Virtual Memory for KV Tensors)**:
   * Instead of allocating large contiguous memory blocks (causing severe memory fragmentation and OOM on mobile devices), PagedAttention allocates non-contiguous physical memory pages in fixed-size blocks (e.g., 16 tokens/block), referenced via a page lookup table.
3. **Speculative Decoding (Draft-Target Speculation)**:
   * A small, ultra-fast draft model (e.g., 1B parameter model running at 80 tokens/sec) speculatively drafts $K$ candidate tokens ($\mathrm{t1}, \dots, \mathrm{tK}$).
   * The large 7B target model evaluates all $K$ tokens in a **single forward pass** using parallel prefill (compute-bound GEMM).
   * Verifies candidates using acceptance sampling. Yields $2\times - 3\times$ speedup on token generation without changing output distribution.

---

# Part 5: Custom Operators, Runtime Frameworks & Tooling

---

### Question 10: Writing Custom Operators in QNN with HVX Intrinsics
> **When a model contains an unsupported operator (or a custom proprietary layer), how do you implement and register a custom QNN package? How do you write optimized HVX SIMD code?**

#### Solution:

#### 1. QNN Custom Op Workflow
1. **Define Op XML Specification**:
   * Create an XML defining inputs, outputs, data types, and attribute parameters.
2. **Generate Package Skeleton (`qnn-op-package-generator`)**:
   * Generates C++ source boilerplate for CPU, GPU, and HTP backends.
3. **Implement Kernel Execution Logic**:
   * Implement `QnnOpPackage_execute()` for the target backend.
4. **Compile Shared Library**:
   * Build `libQnnCustomOpPackageHTP.so` using the Hexagon Clang SDK.
5. **Runtime Registration**:
   * In host code, call `QnnBackend_registerOpPackage()` before instantiating the graph.

#### 2. Writing Optimized HVX SIMD Code
HVX uses 1024-bit vector registers (`HVX_Vector` = 128 bytes = 128 $\times$ `int8` or 64 $\times$ `int16` or 32 $\times$ `int32`).

**Example: Vectorized LeakyReLU Implementation in HVX**:
```c
#include "hexagon_types.h"
#include "hvx_hexagon_protos.h"

// Computes: y = (x > 0) ? x : (x * alpha) for 128 elements of int8
void hvx_leaky_relu_int8(const int8_t* in, int8_t* out, int8_t alpha_q8, int size) {
    HVX_Vector* pIn  = (HVX_Vector*)in;
    HVX_Vector* pOut = (HVX_Vector*)out;
    HVX_Vector v_zero = Q6_V_vzero();
    HVX_Vector v_alpha = Q6_V_vsplat_R(alpha_q8); // broadcast alpha

    for (int i = 0; i < size / 128; i++) {
        HVX_Vector v_x = *pIn++;
        
        // Predicate mask: x > 0
        HVX_VectorPred q_pos = Q6_Q_vcmp_gt_VbVb(v_x, v_zero);
        
        // Multiply negative values with alpha (scale arithmetic)
        HVX_Vector v_neg_scaled = Q6_Vb_vmpy_VbVb(v_x, v_alpha);
        
        // Select between positive x and scaled negative x
        HVX_Vector v_res = Q6_V_vmux_QVV(q_pos, v_x, v_neg_scaled);
        
        *pOut++ = v_res;
    }
}
```

---

### Question 11: PyTorch ExecuTorch & ONNX Runtime QNN Execution Provider
> **How do ExecuTorch and ONNX Runtime integrate with Qualcomm QNN? What is Subgraph Partitioning and Fallback?**

#### Solution:

#### 1. Integration Model
Modern application developers do not write raw QNN C APIs directly; they target standard runtimes like **ExecuTorch** (PyTorch Foundation edge runtime) or **ONNX Runtime (ORT)**.
* Both runtimes use a **Delegation / Execution Provider (EP)** pattern:
  * `QNN Execution Provider (ORT)` / `QNN Backend Delegate (ExecuTorch)`.

#### 2. Subgraph Partitioning & CPU Fallback
```
                       [ Full ONNX / PyTorch Graph ]
                                     |
                       +---------------------------+
                       |    Graph Partitioner      |
                       +---------------------------+
                                     |
                +--------------------+--------------------+
                |                                         |
                v (Supported Subgraph)                    v (Unsupported Nodes)
    +-----------------------+                 +-----------------------+
    |  QNN HTP Delegate     |                 |  Default CPU Runtime  |
    |  (Hexagon NPU Engine) |                 |  (XNNPACK / Native)   |
    +-----------------------+                 +-----------------------+
```
* **Process**:
  1. The runtime queries the QNN backend during initialization: `IsOpSupported(node)`.
  2. The partitioner groups contiguous supported nodes into a single fused `QnnGraph` subgraph.
  3. Non-supported nodes (e.g., custom tokenizers, dynamic string manipulation, rare non-standard layers) are left on CPU.
* **Performance Hazard (Context Switching Overhead)**:
  * Frequent back-and-forth transitions between NPU and CPU incur memory copy and DMA sync overhead across FastRPC.
  * *AE / Optimization Goal*: Refactor unsupported layers or author custom QNN ops to keep 100% of the graph on the NPU.

---

# Part 6: Performance Profiling & Numerical Debugging

---

### Question 12: Debugging Model Accuracy Drop Post-Quantization
> **A customer quantizes an Object Detection model (e.g., YOLOv8) or LLM with AIMET/QNN, but the mAP / Perplexity degrades significantly. Walk through your step-by-step diagnostic methodology.**

#### Solution:

#### Systematic Step-by-Step Diagnostic Methodology:

```
[ FP32 Model Baseline ]  vs.  [ Quantized INT8/INT4 Model ]
                                     |
          +--------------------------+--------------------------+
          |                                                     |
1. Layer-wise SNR Analysis                             2. Calibration Dataset Audit
   - Run layer-by-layer Cosine                            - Check representative coverage
     Similarity & SQNR                                    - Check normalization (0-255 vs 0-1)
          |                                                     |
          +--------------------------+--------------------------+
                                     |
3. Identify Root Cause Layer
   - Sensitive Layers: First Conv, Attention Softmax, SiLU/GELU, Last Regression Head
                                     |
          +--------------------------+--------------------------+
          |                                                     |
4. Apply Targeted Fixes                                5. Mixed Precision Fallback
   - Run AIMET CLE + AdaRound                             - Promote sensitive layer to FP16
   - Adjust Clipping (MSE vs Percentile)                  - Keep rest of backbone in INT8
```

1. **Step 1: Layer-by-Layer Signal-to-Quantization-Noise Ratio (SQNR) & Cosine Similarity**:
   * Use QNN / AIMET accuracy analysis tools to compare intermediate activation tensors between FP32 and INT8:
     $$
\text{SQNR (dB)} = 10 \mathrm{log10} \left( \frac{\sum \mathrm{xFP32}^2}{\sum (\mathrm{xFP32} - \mathrm{xhatINT8})^2} \right)
$$
   * Identify the first layer where SQNR drops below $20\text{ dB}$ or Cosine Similarity drops below $0.98$.
2. **Step 2: Calibration Dataset Sanity Check**:
   * Verify input preprocessing matches production (RGB vs. BGR order, mean/std normalization, dynamic range $[0, 1]$ vs. $[-1, 1]$ vs. $[0, 255]$).
   * Ensure calibration dataset contains representative high-contrast, low-light, and edge-case samples (~500–1000 images).
3. **Step 3: Check Sensitive Structural Points**:
   * **First & Last Layers**: First Convolution (small channel count, high sensitivity) and final classification/bounding-box regression heads often fail in INT8.
   * **Activation Function Saturation**: Check if outlier features are being clipped by using Percentile or MSE calibration instead of MinMax.
4. **Step 4: Remediation Strategies**:
   * Run **AIMET Cross-Layer Equalization (CLE)** to balance dynamic ranges.
   * Apply **AdaRound** on sensitive weight matrices.
   * **Mixed Precision Fallback**: Configure QNN graph config to execute the sensitive layer in **FP16** while running the remaining 95% of the backbone in **INT8** (supported natively on Hexagon HTP).

---

### Question 13: Profiling & Bottleneck Analysis (QNN Profiler & Snapdragon Profiler)
> **How do you profile a deep learning model on Snapdragon hardware? What metrics indicate that a model is compute-bound vs. memory-bound vs. software-overhead bound?**

#### Solution:

#### 1. Profiling Commands & Tools
* Enable QNN profiling during execution:
  ```bash
  qnn-net-run --model libmodel.so --input_list input.txt --profiling_level detailed
  ```
* Generates `QnnProfile.csv` containing cycle counts and execution time per node.
* Use **Snapdragon Profiler** for system-level GPU/NPU/DDR metrics.

#### 2. Key Profiling Metrics & Diagnostics
| Metric in Profile Log | Interpretation | Actionable Optimization |
| :--- | :--- | :--- |
| **High `Total Node Time` with High `HMX Utilization (>80%)`** | **Compute Bound**: Op is fully utilizing matrix ALUs. | Model architecture optimization: prune channels, use smaller kernels, or lower precision (INT4). |
| **High `Total Node Time` with Low `HMX Utilization (<30%)`** | **Memory Bandwidth Bound**: ALU is stalled waiting for DDR RAM data. | Apply layer fusion, increase VTCM residency/tiling, or quantize activations to INT8/FP8. |
| **High `RPC Wait / Sync Time`** | **FastRPC / IPC Overhead**: Host CPU is spending more time marshaling requests than NPU takes to compute. | Batch multiple inferences, enable asynchronous queuing, or use persistent QNN execution loops. |
| **High Number of `Format Conversion / Quantize / Dequantize` Nodes** | **Type Mismatch Overhead**: Graph contains frequent transitions between FP16 and INT8. | Align data types across connected nodes to eliminate standalone re-quantization kernels. |

---

## Final Technical Review Checklist for Qualcomm AI Candidates
* **QNN Core APIs**: `QnnContext_createFromBinary`, `QnnGraph_execute`, `QnnMem_register`.
* **Quantization Formulas**: $S = \frac{\beta - \alpha}{\mathrm{q} - \mathrm{q}}$, $Z = \text{round}\left(\frac{-\alpha}{S}\right) + \mathrm{q}$, Symmetric vs Asymmetric.
* **Hexagon Terms**: HTP, HMX (Matrix), HVX (1024-bit SIMD), VTCM (SRAM), FastRPC, DMA ping-pong.
* **LLM Metrics**: Prefill = Compute Bound (GEMM), Decode = Memory Bandwidth Bound (GEMV), Operational Intensity = $\frac{\text{FLOPs}}{\text{Bytes}}$, KV Cache size calculation.
