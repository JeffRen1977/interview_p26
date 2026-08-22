# MIPI CSI-2 Packed RAW10 解包

相机传感器（MIPI CSI-2）里最经典的数据格式：**Packed RAW10**。每 4 个像素（每个 10-bit，共 40-bit = 5 字节）打包进 5 个连续的 `uint8_t`。

实现与自测：[`unpack_mipi_raw10.cpp`](./unpack_mipi_raw10.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 unpack_mipi_raw10.cpp -o unpack_mipi_raw10 && ./unpack_mipi_raw10
```

---

## 1. 内存打包格式（MIPI CSI-2 RAW10）

4 个连续像素 $P_0, P_1, P_2, P_3$：

| 字节 | 内容 |
|------|------|
| Byte 0 | $P_0[9:2]$ |
| Byte 1 | $P_1[9:2]$ |
| Byte 2 | $P_2[9:2]$ |
| Byte 3 | $P_3[9:2]$ |
| Byte 4 | 四个像素各自的低 2 位（LSBs） |

Byte 4 的 bit 布局：

- bits $[1:0] \to P_0[1:0]$
- bits $[3:2] \to P_1[1:0]$
- bits $[5:4] \to P_2[1:0]$
- bits $[7:6] \to P_3[1:0]$

```
Byte:     0          1          2          3          4
       ┌────────┐┌────────┐┌────────┐┌────────┐┌────────────────────┐
bits:  │ P0[9:2]││ P1[9:2]││ P2[9:2]││ P3[9:2]││ P3[1:0] P2  P1  P0 │
       └────────┘└────────┘└────────┘└────────┘└────────────────────┘
                                                    7:6   5:4 3:2 1:0
```

解包公式：

$$
P_i = (\mathrm{Byte}_i \ll 2) \mid ((\mathrm{Byte}_4 \gg (2 \times i))\ \&\ 0\mathrm{x}03)
$$

输出是 **LSB 对齐** 的 10-bit 值，放在 `uint16_t` 里，范围 $[0, 1023]$。

---

## 2. C++ 高效解包

大分辨率（4K / 8K）要按 **4 像素一块** 展开循环，并且处理行跨度（stride / pitch）：`src_stride_bytes` 往往大于 `ceil(width/4)*5`，ISP 输出几乎从不是 tightly packed。

```cpp
void unpack_mipi_raw10(const uint8_t* __restrict src,
                       uint16_t* __restrict dst,
                       size_t pixel_count);

void unpack_raw10_frame(const uint8_t* src,
                        uint16_t* dst,
                        uint32_t width,
                        uint32_t height,
                        uint32_t src_stride_bytes,
                        uint32_t dst_stride_pixels);
```

约束：`width` / `pixel_count` 必须是 4 的整数倍（MIPI RAW10 的自然对齐）。行起始地址在实机上还要对齐到 64B / cache line（见下面追问）。

---

## 3. 面试官追问（Deep Dive）

写出标量循环之后，Meta 通常会往系统性能上挖：

**SIMD（ARM NEON / AVX2）**  
嵌入式 Cortex-A 上，标量循环撑不住 4K 60fps。NEON（`vld1q_u8`、`vshrq_n_u8`、`vzip`）一次加载 16 字节，解包 12 个像素（16 不是 5 的倍数，所以常见是 16B 里吃 3 个完整 5B 块 = 12 像素，剩 1 字节和下一块拼接）。量级大约 4–8×。

**零拷贝与 DMA 对齐**  
相机驱动 ↔ ISP 走 DMA-BUF / ION。行起始要 64 字节或 cache-line 对齐；解包不要先 memcpy 到临时 buffer，直接从 DMA 映射地址读。

**10-bit vs 16-bit MSB/LSB 对齐**  
上面的实现是 LSB 对齐，输出 $[0, 1023]$。若后面 ISP 流水线要 **MSB 对齐**（16-bit normalized），再 `<< 6`（等价于高 8 位用 `Byte_i`，因为 `(Byte_i << 2) << 6 = Byte_i << 8`），范围变成 $[0, 65472]$。不要误做成 `<< 8` 再 OR 低位，那会把 10-bit 值推出 16-bit。

**Stride 不等于 packed width**  
`src_stride_bytes >= ((width + 3) / 4) * 5`，多出来的是行尾 padding。按行调用 `unpack_mipi_raw10(src_row, dst_row, width)`，下一行用 `src + r * src_stride_bytes`，不要假设整帧是连续的 `height * packed_row`。
