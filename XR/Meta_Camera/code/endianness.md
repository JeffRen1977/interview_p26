# 32-bit Endianness 检测与转换

MIPI 寄存器、EXIF、文件头、跨核 mailbox 都可能和 SoC 的 host endian 不一致。本题：检测本机字节序，并把 `uint32_t` / `uint16_t` 做字节交换。

实现与自测：[`endianness.cpp`](./endianness.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 endianness.cpp -o endianness && ./endianness
```

---

## 1. 什么是大小端

多字节整数在内存里的字节顺序：

| | `0x12345678` 在地址 `p, p+1, p+2, p+3` |
|---|---|
| Little-endian（x86 / 大多数 ARM） | `78 56 34 12` — 低字节在低地址 |
| Big-endian（网络字节序、部分 DSP / 老架构） | `12 34 56 78` — 高字节在低地址 |

检测：把 `uint16_t x = 0x0001` 看成两个字节，低地址是 `0x01` 则是 little-endian。

---

## 2. 交换公式

$$
\mathrm{bswap}_{16}(x) = (x \ll 8) \mid (x \gg 8)
$$

$$
\mathrm{bswap}_{32}(x)=(x \ll 24)\mid((x \ll 8)\ \&\ 0\mathrm{x}00\mathrm{FF}0000)
\mid((x \gg 8)\ \&\ 0\mathrm{x}0000\mathrm{FF}00)\mid(x \gg 24)
$$

等价拆 4 个字节再拼。生产代码可用 `__builtin_bswap32` / `std::byteswap`（C++23）；白板手写掩码。

Host → big-endian（例如写网络 / MIPI 描述符）：本机若已是 BE 则原样返回，否则 `bswap32`。

---

## 3. 追问

- **不要**用 `*(uint32_t*)byte_ptr` 在未对齐地址上读；传感器 dump 常用 `memcpy` 进 `uint32_t`。
- `bswap32(bswap32(x)) == x`。
- 寄存器手册写 “bit 31 is MSB of the 32-bit word” 时，先确认总线是 BE 还是 LE，再谈 bit 编号。
