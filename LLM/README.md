# LLM 面试手撕题

大模型推理相关手写实现，Python（PyTorch）与 C++ 对照。

## 文件

| 文件 | 内容 |
|------|------|
| `kv_cache.py` | KV Cache Prefill + Decode（PyTorch） |
| `kv_cache.cpp` | KV Cache 纯 C++ 实现 |
| `flash_attention.py` | FlashAttention-V1 分块 + Online Softmax（PyTorch） |
| `flash_attention.cpp` | FlashAttention 纯 C++ 实现 |
| `paged_attention.py` | PagedAttention 离散物理块 + 页表寻址（PyTorch） |
| `paged_attention.cpp` | PagedAttention 纯 C++ 实现 |

## 运行

```bash
cd LLM

# Python
python3 kv_cache.py
python3 flash_attention.py
python3 paged_attention.py

# C++
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 或一次跑完全部
cmake --build build --target llm_handwrite
```

## 面试口述要点

1. **KV Cache**：先讲 O(N²) 重复计算；Prefill 建缓存、Decode 只算 1 Token；`torch.cat` vs PagedAttention；Prefill Compute-bound vs Decode Memory-bound。
2. **FlashAttention**：标准 Attention 物化 N×N 矩阵；分块在 SRAM 计算；Online Softmax 跨块对齐 m/d；显存 O(N) vs O(N²)。
3. **PagedAttention**：逻辑块 vs 物理块；block_table 页表寻址；按需 allocate/free；多请求共享 HBM 池，避免 torch.cat 动态扩容。
