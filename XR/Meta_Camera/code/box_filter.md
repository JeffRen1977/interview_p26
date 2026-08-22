# 3×3 均值滤波（Box Filter）与边界 Padding

图像热路径上的 3×3 blur / box filter。考点不是卷积公式，而是 **stride ≠ width** 和 **边界怎么 pad**。

实现与自测：[`box_filter.cpp`](./box_filter.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 box_filter.cpp -o box_filter && ./box_filter
```

---

## 1. 公式

对每个输出像素，取 3×3 邻域均值（整数除法）：

$$
D(r,c)=\left\lfloor\frac{1}{9}\sum_{dy=-1}^{1}\sum_{dx=-1}^{1} S(\mathrm{clamp}(r+dy),\ \mathrm{clamp}(c+dx))\right\rfloor
$$

`clamp` 到图像范围 = **replicate padding**（边缘像素重复）。面试也可说 zero / reflect，先和面试官对齐。

高斯 3×3 只换权重：`[[1,2,1],[2,4,2],[1,2,1]] / 16`（移位代替除法）。

---

## 2. 行跨度

```
src_row = src + r * src_stride     // src_stride >= width
dst_row = dst + r * dst_stride
```

不要写成 `src[r * width + c]`。ISP dump 几乎都有行尾 padding。

整型求和：`9 * 255 = 2295`，用 `int` 即可；不要在 `uint8_t` 上累加。

---

## 3. 追问

- **可分离：** box 是 `1×3` 再 `3×1`，4K 上少乘 3 次。积分图（SAT）把任意窗口变成 4 次加减。
- **SIMD：** 一次处理 16 个像素，边界 lane 仍要 clamp。
- 就地滤波会读到已经写过的像素 → 用双 buffer，或至少一行 line buffer。
