# Vision Algorithm 面试手撕题参考

纯 NumPy 实现，适合白板/在线 coding。面试时优先写**清晰正确**的双循环版，再提优化版。

## 文档

- [手撕代码指南](../docs/04-手撕代码指南.md)
- [算法数学公式](../docs/03-算法数学公式.md)
- [PICO 面试题库](../docs/02-PICO面试题库.md)

## 文件

| 文件 | 内容 |
|------|------|
| `conv2d.py` | 单通道/多通道卷积、im2col 向量化 |
| `nms.py` | IoU、NMS、按类 NMS、Soft-NMS |
| `bilinear_interp.py` | 单点双线性采样、图像 resize、grid_sample |
| `tensor_ops.py` | ReLU/Softmax/BN、池化、Linear、im2col 卷积、Attention |
| `bounded_blocking_queue.py` | 线程安全有界阻塞队列 |
| `spsc_ring_buffer.py` | 单生产者单消费者 Ring Buffer |
| `thread_safe_ring_buffer.py` | 线程安全 Ring Buffer |
| `lru_cache_ds.py` | LRU Cache（双向链表 + 哈希表） |
| `object_pool.py` | 对象池（buffer 复用） |
| `engineering_ds.py` | 运行以上全部测试 |

**LeetCode 分题 Python/C++：** 见 [`../leetcode/`](../leetcode/)（每题 `solution.py` + `solution.cpp`）。

**LLM 手撕（KV Cache / FlashAttention）：** 见 [`../LLM/`](../LLM/)。

**工程 DS C++ 版：** 与下方 Python 文件同目录，一一对应，适合白板面试。

| Python | C++ |
|--------|-----|
| `bounded_blocking_queue.py` | `bounded_blocking_queue.cpp` |
| `spsc_ring_buffer.py` | `spsc_ring_buffer.cpp` |
| — | `mpmc_ring_buffer.cpp` |
| `thread_safe_ring_buffer.py` | `thread_safe_ring_buffer.cpp` |
| `lru_cache_ds.py` | `lru_cache_ds.cpp` |
| `object_pool.py` | `object_pool.cpp` |
| — | `shared_ptr.cpp` |

## 运行

```bash
cd interview_handwrite
python3 conv2d.py
python3 nms.py
python3 bilinear_interp.py
python3 tensor_ops.py
python3 engineering_ds.py          # 全部工程 DS 测试
python3 bounded_blocking_queue.py    # 单题
```

### C++ 工程 DS

```bash
cd interview_handwrite
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 或一次跑完全部工程 DS
cmake --build build --target engineering_ds
```

## 面试口述要点

1. **卷积**：先写单通道二重循环；说明 padding/stride 对输出尺寸的影响。
2. **NMS**：先写 IoU；按 score 降序贪心抑制；说明 per-class NMS。
3. **双线性插值**：写出 4 邻域权重；说清 center-aligned vs corner-aligned。
4. **Tensor**：Softmax 减最大值防溢出；im2col 把卷积变矩阵乘。
5. **工程 DS**：队列用 `while` 防虚假唤醒；SPSC 只一个线程写 tail；LRU = 哈希表 + 双向链表。
