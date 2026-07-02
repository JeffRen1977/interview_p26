# Part 10 — STL 底层原理

> 示例代码：[`examples/09_stl_internals.cpp`](../examples/09_stl_internals.cpp)  
> LeetCode 实现：[`../leetcode/`](../leetcode/)

---

## vector（★★★★★）

### 核心结构

```cpp
template <typename T>
class vector {
    T* data_;        // 连续堆内存
    size_t size_;    // 元素个数
    size_t capacity_; // 已分配槽位（≥ size）
};
```

### 关键操作

| 操作 | 行为 | 复杂度 |
|------|------|--------|
| `push_back(x)` | size+1；若 size==capacity 则扩容 | **均摊 O(1)** |
| `reserve(n)` | 只扩 capacity，不改变 size | O(n) |
| `resize(n)` | 改变 size；不足则默认构造新元素 | O(n) |
| `operator[]` | 随机访问 | O(1) |

### 为什么 push_back 均摊 O(1)？

扩容策略通常是 **2 倍**（或 1.5 倍）：

```
插入 1..n 个元素的总拷贝次数 ≈ n + n/2 + n/4 + ... < 2n
均摊每次 ≈ 2 次拷贝 → O(1) amortized
```

单次扩容 O(n)，但扩容频率指数下降。

### reserve vs resize

```cpp
vector<int> v;
v.reserve(100);  // capacity=100, size=0，元素未构造
v.resize(100);   // capacity≥100, size=100，新元素值初始化/默认构造
```

### push_back vs emplace_back

```cpp
v.push_back(string("hello"));  // 构造临时 string → 移动/拷贝进 vector
v.emplace_back("hello");       // 在 vector 内存上直接构造，少一次移动
```

### 迭代器失效

- `push_back` / `resize` 导致 **reallocation** → 所有迭代器、指针、引用失效
- `erase` → 被删及之后迭代器失效

---

## unordered_map（★★★★★）

### 底层：开链哈希表（bucket + linked list / open addressing）

```
bucket[i] → [key1,val1] → [key3,val3] → null
bucket[j] → [key2,val2] → null
```

| 概念 | 说明 |
|------|------|
| **Hash** | `std::hash<Key>` 计算 hash 值 |
| **Bucket** | `hash % bucket_count` 选桶 |
| **Collision** | 不同 key 落同一桶 → 链地址法或探测 |
| **Load Factor** | `size / bucket_count`；超过 `max_load_factor`（默认 1.0）→ **rehash** |
| **Rehash** | 桶数增加，所有元素重新分布 → O(n)，迭代器失效 |

### 复杂度

- 平均：查找/插入/删除 **O(1)**
- 最坏（全碰撞）：**O(n)**

### 自定义 key

```cpp
struct Point { int x, y; };
struct PointHash {
    size_t operator()(const Point& p) const {
        return hash<int>()(p.x) ^ (hash<int>()(p.y) << 1);
    }
};
unordered_map<Point, int, PointHash> m;
```

---

## map（★★★★★）

### 底层：红黑树（自平衡 BST）

| 操作 | 复杂度 |
|------|--------|
| insert / find / erase | **O(log n)** |
| 遍历 | 有序（按 key 排序） |

**为什么 O(log n)？** 树高 h ≈ log₂(n)；红黑树保证 h ≤ 2log(n+1)。

**vs unordered_map：**

| | map | unordered_map |
|---|-----|---------------|
| 有序 | ✅ | ❌ |
| 平均速度 | 较慢 | 更快 |
| 最坏 | O(log n) 稳定 | O(n) |
| 内存 | 树节点指针开销 | bucket 数组 + 节点 |

---

## deque（★★★★☆）

### 为什么 push_front 也是 O(1)？

不像 vector 单块连续内存，deque 是 **分段连续**（chunk 数组 + 中央 map）：

```
map → [chunk0][chunk1][chunk2]...
       ↑ front      ↑ back
```

两端增长只需在对应 chunk 加元素；中央 map 满时扩容 map（均摊 O(1)）。

- `push_back` / `push_front`：均摊 O(1)
- 随机访问 `operator[]`：O(1)（两次间接）
- 中间插入：O(n)

**用途：** BFS 队列、双端队列、需要两端 pop 的场景。

---

## string — Small String Optimization (SSO)

短字符串**不堆分配**，数据存在对象内部 buffer：

```
// 典型 libc++ / libstdc++（64-bit）
sizeof(string) == 24 or 32 bytes

短串: [size][capacity][inline char buf[15+1]]  // 数据在对象内
长串: [ptr][size][capacity]                    // ptr 指向堆
```

**面试答：** SSO 避免 `malloc` 开销，对短 key、短路径字符串显著提速。

---

## 其他常考

| 容器 | 底层 | 要点 |
|------|------|------|
| `list` | 双向链表 | 插入 O(1)，无随机访问 |
| `set` | 红黑树 | 有序唯一 |
| `priority_queue` | 堆（通常 vector） | top O(1)，push/pop O(log n) |

---

## 与 LeetCode 对应

| 题 | 容器 |
|----|------|
| LRU Cache | `list` + `unordered_map` → [`leetcode/lru_cache`](../leetcode/lru_cache/) |
| Top K | `priority_queue` → [`leetcode/top_k_frequent`](../leetcode/top_k_frequent/) |
| Merge Intervals | `vector` + sort → [`leetcode/merge_intervals`](../leetcode/merge_intervals/) |
