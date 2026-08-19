# 自定义内存对齐分配器（`aligned_malloc` / `aligned_free`）

底层系统编程、DMA 缓冲区、SIMD 数据对齐的经典题。标准 `malloc` **不保证**按任意 2 的幂（16 / 64 / 128 字节）对齐。

实现与自测：[`aligned_malloc.cpp`](./aligned_malloc.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 aligned_malloc.cpp -o aligned_malloc && ./aligned_malloc
```

相机语境：NEON 要 16B 对齐，cache-line / DMA 描述符常要 64B。行 buffer 没对齐会变成拆线 load，或直接被 IOMMU 拒掉。

---

## 1. 核心设计

1. **多申请一段：** `bytes + alignment - 1 + sizeof(void*)`
   - `alignment - 1`：从任意 `raw` 出发，一定能再走到一个对齐地址
   - `sizeof(void*)`：在对齐地址正前方藏原始 `malloc` 指针，给 `aligned_free` 用
2. **对齐公式：**

$$
\text{aligned\_addr} = (\text{raw\_addr} + \text{sizeof(void*)} + \text{alignment} - 1)\ \&\ \sim(\text{alignment} - 1)
$$

3. **回存原始指针：** `((void**)aligned_ptr)[-1] = raw_ptr`
4. **释放：** `aligned_free` 读出该指针，再 `free(raw_ptr)`。对 `aligned_ptr` 直接 `free` 是堆损坏。

`alignment` 必须是 2 的幂；若小于 `sizeof(void*)`，提到指针宽度（否则藏指针本身可能未对齐，部分架构上是 UB）。

---

## 2. 实现要点

```cpp
void* aligned_malloc(size_t bytes, size_t alignment);
void  aligned_free(void* aligned_ptr);   // nullptr 安全
```

```
[ 底层 malloc 的连续块 ]
+-------------------+----------------+-------------------------------+
| 浪费的对齐填充      | 存储 raw_ptr   | 返回给用户的 buffer (bytes)      |
+-------------------+----------------+-------------------------------+
^ raw_ptr           ^ aligned-8      ^ aligned_ptr（alignment 对齐）
```

64-bit 上 `sizeof(void*)==8`。填充长度在 `[0, alignment-1]`，取决于 `malloc` 返回的地址。

---

## 3. 面试官追问

**为什么不能 `realloc`？**  
标准 `realloc` 不知道你藏了 header，可能把块挪走且不再对齐。要对齐扩容：新块 `aligned_malloc` → `memcpy` → `aligned_free` 旧块。

**标准接口（生产用这些，白板仍要手写）：**

| API | 注意 |
|-----|------|
| C11 `aligned_alloc(align, size)` | `size` 必须是 `align` 的整数倍 |
| POSIX `posix_memalign` | 成功返回 0；`free` 即可，不必配对特殊 free |
| Windows `_aligned_malloc` / `_aligned_free` | 必须配对，不能混用 `free` |
| C++17 `operator new(size, align_val_t)` | 对应 `operator delete(p, align_val_t)` |

**溢出：** `bytes + alignment - 1 + sizeof(void*)` 可能绕回，先做减法饱和检查再 `malloc`。

**`aligned_free` 能对普通 `malloc` 指针调用吗？** 不能。`[-1]` 读到的不是合法 heap 指针。
