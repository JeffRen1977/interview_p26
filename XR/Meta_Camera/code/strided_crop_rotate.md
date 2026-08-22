# 2D Strided Crop / Rotate 90° / Flip

ISP 输出**几乎从不是紧密排布的**：`stride > width * bpp`。这题考的不是旋转公式，是你会不会在真实 buffer 布局上写指针。

实现与自测：[`strided_crop_rotate.cpp`](./strided_crop_rotate.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 strided_crop_rotate.cpp -o strided_crop_rotate && ./strided_crop_rotate
```

---

## 1. Crop：先问要不要拷贝

```
src + y * src_stride + x * bpp     // ROI 左上角，零拷贝 view
```

如果下游能接受 `stride`，crop **一次内存都不用碰**。只有下游要求紧密排布时才逐行 `memcpy`。面试时先说这一句，再写循环——这是 senior 和 junior 的分界。

## 2. Rotate 90° CW

输出尺寸是 `(h, w)`，不是 `(w, h)`。映射：

$$
\mathrm{dst}(c,\ h-1-r) = \mathrm{src}(r,\ c)
$$

CCW 是 `dst(w-1-c, r) = src(r, c)`。

**追问：cache 怎么办。** 朴素写法读连续、写跨 `dst_stride`，每次写都换 cache line。分块（tile）成 32×32 后，一块内的写落在同一小片区域：

```
for (r0 = 0; r0 < h; r0 += 32)
  for (c0 = 0; c0 < w; c0 += 32)
    ... // 块内两层循环
```

`.cpp` 里的 tiled 版本和朴素版本 bit-identical，可以直接拿来讲。

## 3. Flip 可以原地，Rotate 不行

| 操作 | 原地？ | 为什么 |
|------|--------|--------|
| 水平翻转 | 可以 | 行内对称交换 |
| 垂直翻转 | 可以 | 整行交换，一行的临时 buffer |
| 旋转 90° | 不可以（非方阵） | 尺寸变了；方阵可以按环 4 元素轮换 |
| 旋转 180° | 可以 | 等价于首尾像素两两交换 |

## 4. 边界与陷阱

- padding 区**不要碰**：测试里 padding 填 `0xEE`，翻转后必须还是 `0xEE`。真机上那块可能是别的 plane 或者未映射页。
- `size_t` 转换：`r * stride` 在 4K×`int` 下接近溢出，先转 `size_t`。
- 多 plane（NV12）要**分别旋转 Y 和 UV**，且 UV 的旋转要保持 2×2 配对，不能按字节转。

## 5. 追问清单

- 90° 旋转能不能让 GPU / MDP / DMA 引擎做？（能——移动 SoC 上显示控制器通常自带 rotator，CPU 旋转 4K 是最后手段）
- EXIF orientation：很多时候根本不该旋转像素，改 metadata 就行。
- 和 `memmove` 的关系：原地翻转是 overlap 拷贝的特例。
