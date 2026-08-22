# NV12 (YUV420sp) → RGB888

ISP 后端给你的就是 NV12。这题的三个考点：**stride、chroma siting、range**。转换矩阵反而是最不重要的部分（可以问面试官要）。

实现与自测：[`nv12_to_rgb.cpp`](./nv12_to_rgb.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 nv12_to_rgb.cpp -o nv12_to_rgb && ./nv12_to_rgb
```

---

## 1. 内存布局

```
Y  plane:  height     行 × y_stride  字节      y_stride  >= width
UV plane:  height/2   行 × uv_stride 字节      uv_stride >= width
```

UV 是**交织**的，一对 (U,V) 覆盖 2×2 的亮度块：

```
Y00 Y01 | Y02 Y03        UV plane 行 0:  U0 V0 | U1 V1
Y10 Y11 | Y12 Y13
--------+--------
Y20 Y21 | Y22 Y23        UV plane 行 1:  U2 V2 | U3 V3
```

所以像素 `(r, c)` 的色度索引是：

```c
uv_row = uv_plane + (r >> 1) * uv_stride;
u = uv_row[(c >> 1) * 2 + 0];
v = uv_row[(c >> 1) * 2 + 1];
```

注意两个 plane 的 stride **可以不相等**，而且 UV plane 的行数是 `height/2` 不是 `height`。

**NV12 vs NV21：** 唯一区别是 U/V 的先后。NV21 是 V 在前（Android 的 `ImageFormat.NV21`）。写成一个 `u_off / v_off` 参数，别写两份函数。

## 2. Range：video 还是 full

| | Y 范围 | UV 范围 | 常见来源 |
|---|--------|---------|----------|
| Video (limited) | 16–235 | 16–240 | 相机 HAL、H.264 默认 |
| Full | 0–255 | 0–255 | JPEG/JFIF、部分 GPU 路径 |

BT.601 video range 整数形式（8 位小数）：

$$
\begin{aligned}
R &= (298(Y-16) + 409(V-128) + 128) \gg 8\\
G &= (298(Y-16) - 100(U-128) - 208(V-128) + 128) \gg 8\\
B &= (298(Y-16) + 516(U-128) + 128) \gg 8
\end{aligned}
$$

`+128` 是为了 `>> 8` 时四舍五入而不是截断。**必须 clamp** 到 `[0,255]`：合法的 YUV 三元组能算出 `R = 280`。

还有 BT.709（HD）和 BT.2020（HDR）系数不同。面试时说一句「这里用 601，709 只是换系数矩阵」就够。

## 3. 白板必答

- [ ] 两个 plane，两个独立 stride，UV 行数 = `height/2`
- [ ] `(r>>1, c>>1)` 定位色度，`*2` 因为交织
- [ ] 奇数宽高：`(width+1)/2` 个色度采样，不要越界
- [ ] clamp，且 `+128` 圆整
- [ ] video vs full range 先问清楚

## 4. 追问

- **反向 RGB→NV12：** 色度要 2×2 box 下采样再算 U/V，不能只取左上角像素（`.cpp` 里有）。
- **性能：** 每像素 3 次乘加，4K30 是 GPU shader 或者 NEON 的活；CPU 单线程标量做 4K 一定超时。真机上这一步应该由 ISP/GPU 完成，CPU 版本只做 fallback。
- **Chroma siting：** MPEG-2 / H.264 的色度采样点在左上像素中心（co-sited horizontally），JPEG 在 2×2 中心。做双线性上采样时会差半个像素——预览看不出，做色度关键的算法（绿幕、肤色检测）会出问题。
