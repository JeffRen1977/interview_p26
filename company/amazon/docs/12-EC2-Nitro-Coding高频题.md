# Part 22 — AWS EC2 Nitro / NX Platforms Coding 高频题

面向 **EC2 Nitro / NX Platforms** 等底层系统岗三轮电话面。  
这类团队很少考深 DP / 复杂图论，而是考：**LLD、指针/内存、网络向字符串与队列**。

> **通关潜规则（比算法更重要）：**  
> 1. RAII / 内存安全：`new`/`malloc` 必有对称释放；能用 `unique_ptr` 就用。  
> 2. 防御性编程：空指针、长度、对齐、溢出先校验。  
> 3. 主动讲 Trade-off：复杂度 + Cache / 锁竞争 / NUMA 视角。

---

## 题库总览 ↔ 仓库实现

| # | 题型 | 核心考点 | 仓库位置 |
|---|------|----------|----------|
| 1 | LRU Cache | 手写双向链表 + map，`O(1)` get/put | [`interview_handwrite/lru_cache_raw_list.cpp`](../../interview_handwrite/lru_cache_raw_list.cpp) · [`lru_cache_ds.cpp`](../../interview_handwrite/lru_cache_ds.cpp)（`std::list::splice`） |
| 2 | Aligned Malloc | 指针算术、多分配、原始指针藏在对齐地址前 | [`examples/14_aligned_malloc.cpp`](../examples/14_aligned_malloc.cpp) |
| 3 | memcpy / memmove | 重叠方向、8 字节对齐拷贝 | [`examples/12_memmove.cpp`](../examples/12_memmove.cpp) · [10-memcpy与memmove.md](./10-memcpy与memmove.md) |
| 4 | Ring Buffer | `(tail+1)%n`；容量 2 的幂用 `& (n-1)` | [`spsc_ring_buffer.cpp`](../../interview_handwrite/spsc_ring_buffer.cpp) · [`mpmc_ring_buffer.cpp`](../../interview_handwrite/mpmc_ring_buffer.cpp) |
| 5 | Token Bucket | 惰性求值，禁后台 timer 线程 | [`examples/13_token_bucket_rate_limiter.cpp`](../examples/13_token_bucket_rate_limiter.cpp) · [11-令牌桶限流器.md](./11-令牌桶限流器.md) |
| 6 | Valid IP | 纯指针扫描，前导零 / 段数 / hex | [`examples/15_valid_ip.cpp`](../examples/15_valid_ip.cpp) |
| 7 | Min-Heap | 数组完全二叉树，`siftUp`/`siftDown` | [`examples/16_min_heap.cpp`](../examples/16_min_heap.cpp) |
| 8 | BitMap | `1<<(val%8)` 标记 40 亿整数存在性 | [`examples/17_bitmap.cpp`](../examples/17_bitmap.cpp) |
| + | 两级 Mempool | Global CAS + Local Cache（加分） | [`two_level_mempool.cpp`](../../interview_handwrite/two_level_mempool.cpp) · [09-无锁固定大小内存池.md](./09-无锁固定大小内存池.md) |

---

## 一、内存管理与缓存设计（出镜率最高）

### 1. LRU Cache

**题：** 设计 LRU，`get` / `put` 均为 $O(1)$。

**必写结构：**

```text
unordered_map<key, Node*>
        +
双向链表：MRU = head->next … LRU = tail->prev
哨兵 head/tail → 消除空表/单节点分支
```

**口述要点：**

- Hit：改值（如需）+ `moveToHead`（`remove` + `addToHead`，只改 4 个指针）。  
- Miss 且满：删 `tail->prev`，同步 `map.erase` + `delete`。  
- 显式 `delete`：系统岗看内存安全意识。

**追问 A — 多线程分段锁：**

```text
shard = hash(key) % N
每段独立：mutex + 局部 LRU
跨段无共享链表 → 锁竞争降到约 1/N
代价：全局精确 LRU 变成“分段近似 LRU”
```

**追问 B — `std::list::splice`：**  
用标准库时，`splice` 只改指针不重新分配节点；手写版等价于 `removeNode` + `addToHead`。见 `lru_cache_ds.cpp`。

---

### 2. Aligned Malloc

**题：** `aligned_malloc(size, alignment)` / `aligned_free(ptr)`，返回地址是 `alignment` 的整数倍（如 64）。

**核心技巧：**

```text
多申请：size + alignment - 1 + sizeof(void*)
在对齐地址前方 sizeof(void*) 处存“原始 malloc 指针”
free 时先读出原始指针再 free
```

```text
[ raw_ptr | padding | aligned payload ... ]
              ↑
         用户拿到的 ptr
         *(void**)(ptr - sizeof(void*)) == raw_ptr
```

**防坑：** `alignment` 必须是 2 的幂；`size==0` / 空指针防御；不要对用户指针直接 `free`。

---

## 二、指针操作与底层拷贝

### 3. memcpy / memmove

| | memcpy | memmove |
|--|--------|---------|
| 重叠 | 禁止（UB） | 必须正确 |

**方向：** `dest ∈ (src, src+n)` → **从后往前**，否则从前往后。  
**加速：** 两端都 8 字节对齐 → `uint64_t` 块拷 + 字节尾巴。  
**追问：** 未对齐在 x86 可能只是慢，在部分 ARM 会 fault；C++ 里未对齐宽访问是 UB。

详见 [10-memcpy与memmove.md](./10-memcpy与memmove.md)。

---

### 4. Circular Ring Buffer

**题：** 固定容量 FIFO，存网包描述符指针。

**白板版：**

```cpp
full  = (tail + 1) % cap == head
empty = head == tail
// 或预留一槽区分满/空（本仓库 SPSC 做法）
```

**加分句：** 容量取 $2^k$，用 `index & (cap-1)` 代替 `%`。  
**进阶：** SPSC 用 acquire/release 无锁；MPMC 用 per-slot sequence + CAS（Vyukov）。

---

## 三、网络流控与数据包解析

### 5. Token Bucket Rate Limiter

**禁区：** 后台 timer 线程刷令牌（百万 pps 下既不准又拖垮流水线）。

**正解 — 惰性求值：**

$$
\text{tokens} = \min(\text{capacity},\; \text{tokens} + \Delta t \times \text{rate})
$$

每次 `AllowPacket(now)` 用硬件时间戳差补令牌；`tokens` 用 `double` 防微秒级截断。

**追问：** 多核 → RSS 绑核（无锁）或 per-core local bucket。

---

### 6. Valid IP Address

**题：** 判断字符串是 IPv4、IPv6，还是 Neither。  
**约束：** 不用 `sscanf` / 正则，纯指针扫描。

**IPv4 硬规则：**

- 恰好 4 段，`.` 分隔；  
- 每段 0–255；  
- **无前导零**（`"0"` 合法，`"01"` / `"00"` 非法）；  
- 无空段。

**IPv6 硬规则（常考简化版）：**

- 恰好 8 组，`:` 分隔；  
- 每组 1–4 个 hex；  
- 大小写均可；  
- 面试常不要求压缩形式 `::`（若要求需单独状态机）。

---

## 四、经典结构与位运算

### 7. Min-Heap（优先队列）

**网络背景：** 按时间戳 / QoS 优先级调度。

**数组下标：**

$$
\text{parent}(i)=\lfloor(i-1)/2\rfloor,\quad
\text{left}=2i+1,\quad
\text{right}=2i+2
$$

`push` → 尾插 + `siftUp`；`pop` → 顶换尾 + `siftDown`。

---

### 8. BitMap

**题：** ~32MB 内存标记/查询 40 亿不重复 `uint32` 是否出现过。

$$
40\text{亿 bit} \approx 0.5\text{GB}
\quad(\text{若题面是 32 位全集 }2^{32}\text{ bit}=512\text{MB})
$$

实现：

```cpp
bytes[val >> 3] |=  (1u << (val & 7));  // set
bool on = bytes[val >> 3] & (1u << (val & 7));  // test
```

口述：1 bit/key，远小于 `unordered_set`；无哈希碰撞；随机访问要注意 TLB/Cache。

---

## 五、电话面答题节奏（推荐）

```text
1. 复述题 + 约束（线程模型？容量？错误返回？）
2. 先写正确暴力/清晰版，再提优化（对齐、位运算、2^n）
3. 边写边说复杂度与边界用例
4. 写完主动：
   - 空指针 / 溢出 / 异常路径谁负责 free
   - Trade-off（锁 vs 无锁、精确 LRU vs 分段近似）
   - 数据面落地：描述符 vs 大 payload、Cache Line、RSS
```

### 60 秒自我介绍式收尾模板

> 这版时间 $O(1)$/ $O(\log n)$，空间 $O(n)$。若在 Nitro 数据面热路径，我会避免在主循环 `malloc`，改用预分配 mempool；多核用 RSS 绑核或分段锁降竞争；并强调所有权——谁分配谁释放，或用 `unique_ptr`/池化消除泄漏窗口。

---

## 六、三轮电话建议刷题顺序

| 优先级 | 题目 | 理由 |
|--------|------|------|
| P0 | LRU、Ring Buffer、memmove、Token Bucket | 几乎必考 |
| P1 | Aligned Malloc、Valid IP | 指针/边界功底 |
| P2 | Min-Heap、BitMap、两级 Mempool | 优化与扩展加分 |

每天口述 + 默写一题；默写时关掉 IDE 补全，专门练哨兵节点、CAS、对齐公式。
