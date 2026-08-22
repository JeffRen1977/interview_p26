# 位级环形移位（Circular Shift / Bit Rotation）

环形移位常用于加密哈希（SHA-256）、伪随机数生成、底层通信编解码。与逻辑移位（丢弃溢出位、补 0）不同，移出的位会从另一端补回来。

实现与自测：[`bit_rotation.cpp`](./bit_rotation.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 bit_rotation.cpp -o bit_rotation && ./bit_rotation
```

---

## 1. 核心数学原理

对 $N$-bit 无符号整数 $x$ 循环左移 $k$ 位（$\mathrm{ROTL}(x, k)$）：移出的高 $k$ 位接到最低位。

$$
\mathrm{ROTL}_N(x, k) = (x \ll k) \mid (x \gg (N - k))
$$

$$
\mathrm{ROTR}_N(x, k) = (x \gg k) \mid (x \ll (N - k))
$$

```
x      =  1 0 1 1 0 0 1 0     N=8, k=3
ROTL   =  1 0 0 1 0 1 0 1     高 3 位 101 接到最低位
ROTR   =  0 1 0 1 0 1 1 0     低 3 位 010 接到最高位
```

**未定义行为（必须先处理 $k$）：**

C/C++ 里移位位数 $\ge N$ 或为负都是 UB。特别地 $k = 0$ 时 $N-k = N$，写 `(x >> N)` 也是 UB。健壮实现要对 $k$ 做模 $N$ 掩码：`k &= (N - 1)`（要求 $N$ 是 2 的幂）。

---

## 2. 标准跨平台实现

### 方案 A：手写位运算（编译器友好）

`(-k) & (N - 1)` 利用无符号补码，避免 $k=0$ 时的分支和 `>> N`：

```cpp
constexpr uint32_t rotl32(uint32_t x, unsigned int k) {
    const unsigned int mask = 31;  // 32 - 1
    k &= mask;
    // k=0 → (-k & 31)==0，变成 (x<<0)|(x>>0)，不会 >> 32
    return (x << k) | (x >> ((-k) & mask));
}

constexpr uint32_t rotr32(uint32_t x, unsigned int k) {
    const unsigned int mask = 31;
    k &= mask;
    return (x >> k) | (x << ((-k) & mask));
}
```

`k` 必须是 **unsigned**：对 unsigned 取负是模 $2^{w}$ 的，行为确定。`rotl32(x, k) == rotr32(x, 32-k)`（$k \bmod 32 \ne 0$ 时）。

### 方案 B：C++20 `<bit>`

```cpp
#include <bit>
uint32_t a = 0x12345678;
uint32_t shifted_left  = std::rotl(a, 4);
uint32_t shifted_right = std::rotr(a, 4);
```

零开销，面试里可以说“生产代码用 `std::rotl`；白板手写方案 A，并点出 UB”。

---

## 3. 硬件指令与编译器内建

现代 CPU 单周期完成任意位数旋转：

| 架构 | 指令 |
|------|------|
| x86 / x86_64 | `ROL` / `ROR` |
| ARM / AArch64 | `ROR`（左旋用负的右旋次数映射） |

```cpp
#if defined(_MSC_VER)
    #define ROTL32(x, k) _rotl((x), (k))
    #define ROTR32(x, k) _rotr((x), (k))
#elif defined(__GNUC__) || defined(__clang__)
    // -O2 下方案 A 会折叠成一条 ROL/ROR
    #define ROTL32(x, k) rotl32((x), (k))
    #define ROTR32(x, k) rotr32((x), (k))
#endif
```

面试金句：不要手写内联汇编；写成方案 A，让编译器选指令。可用 `objdump` / Compiler Explorer 确认 `rol` / `ror`。

---

## 4. 批量处理（ARM NEON）

对连续 `uint32_t` 数组，一次旋转 4 个 lane。`vshl` 的负 shift 表示逻辑右移；lane 宽度以上的移位会清零该 lane，所以 $k=0$ 时不会踩 C 的 `>> 32` UB，但仍应把 `shift` 限制在 $[0, 31]$。

```cpp
#include <arm_neon.h>

inline uint32x4_t rotl32_neon(uint32x4_t val, int shift) {
    uint32x4_t left  = vshlq_u32(val, vdupq_n_s32(shift));
    uint32x4_t right = vshlq_u32(val, vdupq_n_s32(-(32 - shift)));
    return vorrq_u32(left, right);
}
```

相机场景里旋转本身很少是热路径；更常见的是 **Bayer / RAW 打包、哈希（固件签名）、CRC、加扰多项式**。本题考的是：你会不会写出带 UB 的 `(x << k) | (x >> (32-k))`。
