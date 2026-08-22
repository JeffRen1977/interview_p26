# 灰度直方图与直方图均衡（CDF）

统计 8-bit 灰度 `hist[256]`，再由 CDF 做均衡化。相机 AE / 对比度拉伸的白板版。

实现与自测：[`histogram.cpp`](./histogram.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 histogram.cpp -o histogram && ./histogram
```

---

## 1. 直方图与 CDF

$$
h[i]=\#\{p \mid p=i\},\quad
\mathrm{cdf}[i]=\sum_{k=0}^{i} h[k]
$$

`cdf[255] == width * height`（只统计有效像素，不要把 stride padding 算进去）。

---

## 2. 均衡化 LUT

$$
\mathrm{lut}[i]=\mathrm{round}\frac{(\mathrm{cdf}[i]-\mathrm{cdf}_{\min})\times 255}{N-\mathrm{cdf}_{\min}}
$$

`cdf_min` 是第一个非零 CDF（跳过没用到的暗端）。`N` 是像素数。全图常数时分母为 0，LUT 应映射到该灰度本身。

然后 `dst[p] = lut[src[p]]`，仍然按 `src_stride` / `dst_stride` 走。

---

## 3. 追问

- 积分图 SAT：`S[r,c] = p + S[r-1,c] + S[r,c-1] - S[r-1,c-1]`，任意矩形和 O(1)。
- 彩色图通常只均衡 Y，不要分别拉 R/G/B（会偏色）。
- 在 RAW Bayer 上直接均衡会破坏通道比例；应在 demosaic 后的 Y 上做。
