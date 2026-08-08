# Part 20 — `memcpy` / `memmove`：重叠、对齐与流水线

> 白板实现：[`examples/12_memmove.cpp`](../examples/12_memmove.cpp)

亚马逊高级系统岗手写 `memcpy`/`memmove`，考的不是 `for` 循环，而是：

1. **Overlap**：何时必须从后往前拷；
2. **Alignment**：为何以及何时用 `uint64_t` 块拷；
3. **ISA 差异**：未对齐访问在 x86 vs ARM 上的行为；
4. **大数据路径**：SIMD、非临时写、Cache Pollution。

---

## 一、`memcpy` vs `memmove`

| | `memcpy` | `memmove` |
|--|----------|-----------|
| 重叠 | **禁止**（重叠 → UB） | **必须正确处理** |
| 典型用途 | 已知不重叠的缓冲区 | 通用移动、同缓冲区内滑动 |
| 面试策略 | 先说差异，再实现安全的 `memmove` | 实现它就覆盖了两者 |

系统代码里，若无法静态证明不重叠，直接用 `memmove`（或自研带重叠检测的版本）。

---

## 二、重叠检测（生死线）

把区间写成半开区间 `[p, p + count)`。

**需要从后往前拷**的条件：

$$
\text{dest} \in (\text{src},\; \text{src}+\text{count})
$$

即：`dest > src && dest < src + count`。

```text
src:   [ S0 S1 S2 S3 S4 S5 ]
dest:        [ D0 D1 D2 D3 D4 D5 ]
             ↑ dest 落在 src 内部

若从前往后拷：写 D0 会破坏尚未读完的 S2…
必须从末尾往前拷。
```

其他情况（`dest <= src`，或完全不重叠）用从前往后拷即可。

白板口述：

> 先判重叠方向，再谈对齐加速。方向错了，对齐再快也是错的。

---

## 三、字对齐（Word Alignment）

64 位机器上，`uint64_t` 一次搬 8 字节，比逐字节少约 8 倍存储事务（理想情况）。

**安全开启字拷的条件（白板版）：**

- 正向：`dest` 与 `src` **起始地址**都 8 字节对齐；
- 反向：两者的 **结束地址**（`+ count` 后）都 8 字节对齐。

然后：

1. 先用字节循环处理 `count % 8` 的尾巴；
2. 再用 `uint64_t` 循环搬 `count / 8` 块。

```cpp
if ((uintptr_t(d) & 7) == 0 && (uintptr_t(s) & 7) == 0) {
    // 8-byte loop + byte tail
}
```

### 为什么不能“强行”转 `uint64_t*`？

| 平台 | 未对齐 8 字节访问 |
|------|-------------------|
| **x86-64** | 硬件通常允许，但可能更慢（拆分访问 / 跨 cache line） |
| **部分 ARM / 旧架构** | 可能触发 **Alignment Fault**，进程崩溃 |
| **C++ 抽象机** | 对未对齐地址做 `uint64_t*` 解引用属于 **UB** |

因此严谨做法是：

1. 双方都对齐 → 字拷；
2. 只对齐一侧 → 先字节“剥”到对齐边界，或全程字节；
3. 需要可移植未对齐字拷 → 用 `memcpy` 进局部 `uint64_t`，或编译器 intrinsic / `std::bit_cast` 风格，而不是裸解引用。

---

## 四、白板实现骨架

完整代码见示例。核心结构：

```cpp
void* optimized_memmove(void* dest, const void* src, size_t count) {
    if (!dest || !src || count == 0) return dest;

    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d > s && d < s + count) {
        // backward: maybe word-sized from the end
    } else {
        // forward: maybe word-sized from the start
    }
    return dest;
}
```

实现要点：

- 用 `uint8_t*` 做指针运算（`void*` 不能算术）；
- `count & 7` 代替 `count % 8`；
- `__builtin_expect` 仅作分支提示，不是正确性必需。

---

## 五、面试追问速答

### Q1：`src` 对齐、`dest` 不对齐，直接 `uint64_t*` 会怎样？

> x86-64 多半不崩但更慢；部分 ARM 可能 Alignment Fault；C++ 层面是 UB。生产代码先字节对齐到边界，或避免未对齐宽访问。

### Q2：拷几 MB 网络缓冲，如何再榨性能？

分三层说：

1. **SIMD**：AVX2/AVX-512 或 NEON/SVE，单指令搬 32/64B；
2. **Non-temporal store**（如 `_mm512_stream_si512`）：大块一次性流过、以后不再用的数据，绕过 cache 填入，减轻 **Cache Pollution**；
3. **测量**：带宽是否已接近 DRAM；若已饱和，换指令收益有限，应看 TLB（大页）、NUMA、是否该用 DMA/零拷贝。

### Q3：为什么 libc 的 `memmove` 往往比手写快？

> 架构汇编、运行时 CPU 特性分派、精心安排的预取与对齐剥离、SIMD 与 ERMS/`rep movsb` 等。面试手写证明你懂约束；量产优先用 libc / 编译器 builtin（`__builtin_memmove`）。

### Q4：`restrict` 和 `memcpy` 的关系？

> C 的 `restrict` 告诉编译器指针不重叠，便于向量化。`memcpy` 的契约本质上就是“调用方保证不重叠”。C++ 没有同等标准 `restrict`（有扩展），重叠场景用 `memmove`。

### Q5：和 `std::copy` / `std::copy_backward`？

> 对平凡字节类型，标准库常会落到 `memmove`/`memcpy`。手写题要你展示重叠方向选择，等价于：`dest` 在 `src` 右边用 backward，否则 forward。

---

## 六、口述 60 秒版本

> `memcpy` 假定不重叠；系统里我实现 `memmove`。先判断 `dest` 是否落在 `[src, src+n)`：是则从后往前，否则从前往后。两边都 8 字节对齐时用 `uint64_t` 块拷，尾巴用字节补齐，把总线事务降到约 1/8。未对齐不硬转宽指针：x86 可能只是变慢，ARM 可能 fault，C++ 是 UB。超大缓冲再谈 AVX 和非临时写，避免冲刷 L1/L2 热数据。最终仍以 libc 调优实现为准，手写是为了讲清硬件约束。

---

## 七、自测清单

跑示例：

```bash
cd amazon_cpp
cmake --build build --target 12_memmove
./build/12_memmove
```

必须覆盖：

- 无重叠正向；
- `dest` 在 `src` 右侧（backward）；
- `dest` 在 `src` 左侧（forward）；
- 对齐大块 + 非 8 倍数尾巴；
- 与 `std::memmove` 对照。
