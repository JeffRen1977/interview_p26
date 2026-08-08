# 01 - GEMM 底层优化（Cache / SIMD / GPU）

> **手撕代码：** [gemm.cpp](./gemm.cpp) · [gemm.py](./gemm.py)  
> **关联：** [26-Microsoft-Principal-ML-Systems面试准备](../docs/26-Microsoft-Principal-ML-Systems面试准备.md)

针对 Principal 级别面试，面试官期望看到的是你能够将**数学公式**、计算机体系结构（Cache、内存带宽、寄存器）**以及**微架构特性（SIMD/Tensor Cores）完美结合的推演能力。

---

## 1. 为什么经典 $O(n^3)$ 是灾难性的？（Cache Miss 分析）

标准的 3 重循环（$I \to J \to K$）形式如下：

```cpp
for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
        for (int k = 0; k < N; k++)
            C[i][j] += A[i][k] * B[k][j];
```

### 空间局部性（Spatial Locality）分析

| 矩阵 | 访问模式 | Cache 行为 |
|------|----------|------------|
| **A** | `A[i][k]` 按行，内存连续 | Cache 友好 |
| **B** | `B[k][j]` **按列** | Row-major 下每次 `k++` 跳跃 $N$ 个元素；$N$ 大时超出 L1/L2 → **Cache Miss** |
| **代价** | L1 ~4–5 cycles；DRAM Miss ~200+ cycles | CPU ALU 空转，**Memory-bound** |

---

## 2. 第一步优化：改变循环顺序（Loop Permutation）

在不引入分块之前，最简单的优化是改变循环顺序为 $I \to K \to J$：

```cpp
for (int i = 0; i < N; i++)
    for (int k = 0; k < N; k++) {
        float r = A[i][k];  // 缓存到寄存器
        for (int j = 0; j < N; j++)
            C[i][j] += r * B[k][j];
    }
```

- **效果**：最内层 `j` 循环中，`C[i][j]` 和 `B[k][j]` 全都**按行连续访问**
- **收益**：充分利用 Cache Line（64B ≈ 16 个 `float`），通常性能提升**数倍**

---

## 3. 第二步优化：分块矩阵乘法（Tiled/Blocked GEMM）

当矩阵规模远大于 Cache 容量时，仅改变循环顺序无法解决**时间局部性**问题（数据加载后很快被挤出 Cache）。

**分块（Tiling）核心思想**：将大矩阵切分为 $B \times B$ 小块，使一块数据**完全容纳在 L1/L2**，块内 100% 复用。

```cpp
for (int sj = 0; sj < N; sj += BLOCK_SIZE) {
    for (int sk = 0; sk < N; sk += BLOCK_SIZE) {
        for (int si = 0; si < N; si += BLOCK_SIZE) {
            for (int i = si; i < si + BLOCK_SIZE; ++i) {
                for (int k = sk; k < sk + BLOCK_SIZE; ++k) {
                    float r = A[i][k];
                    for (int j = sj; j < sj + BLOCK_SIZE; ++j)
                        C[i][j] += r * B[k][j];
                }
            }
        }
    }
}
```

### 面试官必问：如何确定 `BLOCK_SIZE`？

1. **寄存器级分块（Register Tiling）**：最内层 tile 能放进 CPU 寄存器（x86-64 约 16 个 YMM/ZMM）
2. **Cache 级分块**：$A_{\text{tile}} + B_{\text{tile}} + C_{\text{tile}} \le$ L1 Data Cache（通常 32–48 KB）

$$\text{Size}(A_{\text{tile}}) + \text{Size}(B_{\text{tile}}) + \text{Size}(C_{\text{tile}}) \le \text{L1 Cache Size}$$

---

## 4. 第三步优化：硬件感知与向量化（SIMD / AVX-512）

纯靠编译器 Auto-vectorization 往往无法榨干性能；生产环境依赖 MKL / OpenBLAS 或手写 intrinsics。

AVX-512 寄存器 512 bit，可同时处理 **16 个 float**。内层 `j` 以 16 为步长，用 **FMA** 融合乘加：

```cpp
#include <immintrin.h>

for (int i = si; i < si + BLOCK_SIZE; ++i) {
    for (int k = sk; k < sk + BLOCK_SIZE; ++k) {
        __m512 vec_A = _mm512_set1_ps(A[i][k]);
        for (int j = sj; j < sj + BLOCK_SIZE; j += 16) {
            __m512 vec_B = _mm512_loadu_ps(&B[k][j]);
            __m512 vec_C = _mm512_loadu_ps(&C[i][j]);
            vec_C = _mm512_fmadd_ps(vec_A, vec_B, vec_C);
            _mm512_storeu_ps(&C[i][j], vec_C);
        }
    }
}
```

见 [gemm.cpp](./gemm.cpp) 中 `#ifdef GEMM_AVX512` 分支。

---

## 5. 衍生到 GPU：CUDA Tensor Cores 与分布式切分

Solstice / Frontier 类 GPU 效率优化常延伸到 NVIDIA Ampere/Hopper：

| 层级 | 机制 | 解决的问题 |
|------|------|------------|
| **Thread Block Tiling** | Global Memory → Shared Memory | HBM 带宽瓶颈 |
| **Warp Tiling** | Block 内 32 线程切分 | 并行度 + 寄存器复用 |
| **Tensor Cores (WMMA/MMA)** | 单周期 $16\times16\times16$ FP16/BF16 MAC | Compute-bound 峰值 |

### 分布式 GEMM（Tensor Parallelism）

权重矩阵单卡放不下时：

| 切分 | 做法 | 通信 |
|------|------|------|
| **列切 (Column Parallel)** | $B$ 按列分到多卡 | 前向后 **All-Gather** 拼接 |
| **行切 (Row Parallel)** | $B$ 按行分到多卡 | 局部结果 **All-Reduce (Sum)** |

**通信掩盖：** Compute Stream 与 NCCL Stream **双缓冲流水**，用计算时间掩盖集合通信延迟（见 [22-LLM训练计算通信重叠与MFU优化](../docs/22-LLM训练计算通信重叠与MFU优化.md)）。

---

## 6. 白板估算模板

**问题：** $N=4096$，FP32 GEMM，峰值算力 10 TFLOPS，DRAM 带宽 900 GB/s，Achievable GFLOPS?

- 理论 FLOPs：$2N^3 \approx 137$ GFLOP
- **Arithmetic intensity**（Roofline）：$\frac{2N^3}{3N^2 \cdot 4\text{B}} \approx \frac{2N}{12} \approx 683$ FLOP/Byte
- 若 intensity × 带宽 < 峰值算力 → **Memory-bound**；否则 Compute-bound
- Principal 级答案：先画 Roofline，再说明 blocked + SIMD 如何提高 intensity 与 Cache 命中率

---

## 7. 代码索引

```bash
cd microsoft
python3 gemm.py
cmake -S . -B build && cmake --build build && ./build/gemm
```

| 实现 | 文件 | 考点 |
|------|------|------|
| Naive IJK | `gemm_naive_ijk` | Cache Miss 反面教材 |
| IKJ | `gemm_ikj` | 循环重排 |
| Blocked | `gemm_blocked` | L1 tiling |
| AVX-512 FMA | `gemm_blocked_avx512` | SIMD（`-DGEMM_AVX512 -mavx512f`） |
