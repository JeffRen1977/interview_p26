# Microsoft Principal ML Systems 面试手撕

面向 **Microsoft Principal Software Engineer**（Solstice / Frontier 模型 GPU 效率优化）——第一性原理推演：Cache、内存带宽、SIMD/Tensor Cores。

> **关联文档：** [docs/26-Microsoft-Principal-ML-Systems面试准备.md](../docs/26-Microsoft-Principal-ML-Systems面试准备.md)

## 文档

| 文档 | 内容 |
|------|------|
| [01-gemm-cache-simd.md](./01-gemm-cache-simd.md) | GEMM 循环顺序、分块、AVX-512、GPU Tensor Core、分布式切分 |

## 代码

| 文件 | 内容 |
|------|------|
| `gemm.py` | Naive / IKJ / Blocked GEMM（NumPy 对照） |
| `gemm.cpp` | Naive / IKJ / Blocked / 可选 AVX-512 FMA |

## 运行

```bash
cd microsoft
python3 gemm.py

cmake -S . -B build
cmake --build build
./build/gemm
```

## 面试口述顺序

1. **Naive IJK** — B 按列访问，Row-major 下 Cache Miss 灾难
2. **IKJ 重排** — 内层 j 连续，Cache Line 友好，数倍提升
3. **Blocked GEMM** — L1 容纳 tile，时间局部性；如何选 BLOCK_SIZE
4. **SIMD FMA** — 一条指令 16 float，仍受 Memory-bound 约束
5. **GPU 延伸** — Shared Memory tiling、Tensor Core WMMA、TP 列/行切分 + All-Gather/All-Reduce
