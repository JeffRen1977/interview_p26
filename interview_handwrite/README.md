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

## 运行

```bash
cd interview_handwrite
python3 conv2d.py
python3 nms.py
python3 bilinear_interp.py
python3 tensor_ops.py
```

## 面试口述要点

1. **卷积**：先写单通道二重循环；说明 padding/stride 对输出尺寸的影响。
2. **NMS**：先写 IoU；按 score 降序贪心抑制；说明 per-class NMS。
3. **双线性插值**：写出 4 邻域权重；说清 center-aligned vs corner-aligned。
4. **Tensor**：Softmax 减最大值防溢出；im2col 把卷积变矩阵乘。
