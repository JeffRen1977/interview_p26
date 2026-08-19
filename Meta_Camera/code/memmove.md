# `memcpy` / `memmove`（重叠拷贝）

标准 `memcpy` 在 `dest` 与 `src` 重叠时是 **undefined behavior**。相机行 buffer 原地平移、crop、ring 回绕必须用 `memmove`：先判断重叠，再决定正向还是反向拷贝。

实现与自测：[`memmove.cpp`](./memmove.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 memmove.cpp -o memmove_drill && ./memmove_drill
```

---

## 1. 何时正向 / 反向

把区间看成字节指针 `d`、`s`，长度 `n`：

| 关系 | 拷贝方向 | 原因 |
|------|----------|------|
| `d == s` 或 `n == 0` | 什么都不做 | |
| `d < s` 或 `d >= s + n` | **正向** `i = 0 .. n-1` | 不会覆盖尚未读的源字节 |
| `s < d < s + n` | **反向** `i = n-1 .. 0` | 目标落在源区间内部，正向会先毁掉尾巴 |

```
src:     [ A B C D E F ]
dest = src+2, n=4     → 需要变成 [ A B A B C D ]
正向会先写 dest[0]=A 把原来的 C 毁掉 ❌
反向从 E←D←C←B 写，源还在 ✅
```

---

## 2. 实现要点

- 用 `unsigned char*` 按字节走，满足 strict aliasing。
- 返回 `dest`（与 libc 相同）。
- `memcpy` 可以假设不重叠，实现与正向 `memmove` 相同；面试要先问清是否可能 overlap。
- 生产路径用 libc；本题考的是重叠判定，不是 SIMD 拷贝。

追问：DMA 搬帧用硬件，不要对 4K NV12 走 CPU `memmove`。CPU 版只用于元数据、行内少量平移、测试。
