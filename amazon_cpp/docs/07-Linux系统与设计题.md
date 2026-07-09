# Part 16 / 17 / 18 — Linux、系统设计 C++、Coding

---

## Part 16：Linux（★★★★★）

### Process vs Thread

| | Process | Thread |
|---|---------|--------|
| 地址空间 | 独立 | 共享（除栈、寄存器） |
| 创建开销 | 大（fork + 页表） | 小 |
| 通信 | pipe、socket、共享内存 | 直接读写共享变量（需同步） |
| 崩溃 | 隔离 | 一线程崩可能拖垮进程 |

### fork / exec

```cpp
pid_t pid = fork();
if (pid == 0) {
    execl("/bin/ls", "ls", "-l", nullptr);  // 子进程替换镜像
} else {
    waitpid(pid, nullptr, 0);
}
```

- **fork**：复制（CoW）当前进程
- **exec**：用新程序覆盖当前进程镜像

### I/O 模型

| 机制 | 说明 |
|------|------|
| **Pipe** | 单向字节流；`pipe()` 两 fd |
| **Socket** | 网络/本地 `AF_UNIX` 通信 |
| **mmap** | 文件/匿名映射到地址空间，零拷贝共享 |
| **epoll** | 高并发 I/O 多路复用（边缘/水平触发） |

### epoll 为什么快？

- `select`/`poll`：O(n) 扫描所有 fd
- `epoll`：内核维护就绪列表，O(1) 获取活跃 fd；适合数万连接

```cpp
int epfd = epoll_create1(0);
epoll_event ev{.events = EPOLLIN, .data.fd = listen_fd};
epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
// epoll_wait(epfd, events, MAX, -1);
```

**面试答法：** 数据面用非阻塞 fd + epoll；控制面用 gRPC/HTTP；热路径避免阻塞系统调用。

---

## Part 17：系统设计里的 C++（★★★★★）

本仓库已有完整 C++ 实现 + 白板要点：

| 设计题 | 路径 | 核心考点 |
|--------|------|----------|
| **Blocking Queue** | [`interview_handwrite/bounded_blocking_queue.cpp`](../interview_handwrite/bounded_blocking_queue.cpp) | mutex + 2×CV |
| **SPSC Ring Buffer** | [`interview_handwrite/spsc_ring_buffer.cpp`](../interview_handwrite/spsc_ring_buffer.cpp) | atomic acquire/release |
| **MPMC Ring Buffer** | [`interview_handwrite/thread_safe_ring_buffer.cpp`](../interview_handwrite/thread_safe_ring_buffer.cpp) | mutex |
| **LRU Cache** | [`interview_handwrite/lru_cache_ds.cpp`](../interview_handwrite/lru_cache_ds.cpp) | list + unordered_map |
| **Object Pool** | [`interview_handwrite/object_pool.cpp`](../interview_handwrite/object_pool.cpp) | placement new / 复用 |
| **Thread Pool** | 见下方模式 | queue + worker threads |

### Thread Pool 骨架（常考）

```cpp
class ThreadPool {
    std::vector<std::thread> workers_;
    BoundedBlockingQueue<std::function<void()>> tasks_;
    std::atomic<bool> stop_{false};

public:
    explicit ThreadPool(size_t n) : tasks_(1024) {
        for (size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] {
                while (!stop_) {
                    if (auto task = tasks_.try_get()) (*task)();
                }
            });
        }
    }
    template <typename F>
    void submit(F&& f) { tasks_.put(std::forward<F>(f)); }
    ~ThreadPool() {
        stop_ = true;
        for (auto& t : workers_) t.join();
    }
};
```

**口述：** 任务队列解耦提交与执行；`stop` 用 atomic；析构 join 所有线程；任务类型用 `std::function` 或类型擦除。

### 系统设计与 C++ 结合（AWS）

见 [`docs/17-AWS-EC2-Nitro-系统设计.md`](../../docs/17-AWS-EC2-Nitro-系统设计.md)：
- Host Agent ↔ Nitro mailbox ≈ SPSC ring buffer
- GPU 健康监控 ≈ 事件驱动 + 状态机
- ML 调度 ≈ gang scheduling + 拓扑感知

---

## Part 18：Coding（★★★★★）

### 难度与策略

- **Medium 为主**；Hard 偶尔
- Amazon 更看重 **代码质量**：const、RAII、边界、复杂度口述
- 本仓库 [`leetcode/`](../leetcode/) 每题 `solution.cpp` + `solution.py`

### 高频题型

| 类型 | 例题 | 路径 |
|------|------|------|
| 哈希 | Two Sum | `leetcode/two_sum/` |
| 设计 | LRU Cache | `leetcode/lru_cache/` |
| 区间 | Merge Intervals | `leetcode/merge_intervals/` |
| 二分 | Search Rotated Array | `leetcode/search_rotated_array/` |
| 图 DFS/BFS | Number of Islands | `leetcode/number_of_islands/` |
| 堆 | Top K Frequent | `leetcode/top_k_frequent/` |
| 滑动窗口 | Longest Substring | `leetcode/longest_substring_without_repeating/` |
| 链表 | Reverse Linked List | `leetcode/reverse_linked_list/` |
| 树 | LCA | `leetcode/lowest_common_ancestor/` |

### 面试书写顺序

1. 澄清输入输出、边界
2. 暴力思路 → 优化思路 + 复杂度
3. 写代码（先正确）
4. _walk through_ 例子
5. 提优化/ follow-up

### C++ 编码规范（Amazon 加分）

```cpp
class Solution {
public:
    std::vector<int> twoSum(const std::vector<int>& nums, int target) {
        std::unordered_map<int, int> seen;  // value → index
        for (int i = 0; i < static_cast<int>(nums.size()); ++i) {
            auto it = seen.find(target - nums[i]);
            if (it != seen.end()) return {it->second, i};
            seen[nums[i]] = i;
        }
        return {};
    }
};
```

- 用 `const&` 传大对象
- 用 `unordered_map` 代替 `map` 当不需要有序
- 避免裸 `new`；用 `vector`、智能指针

---

## 模拟面试：30 分钟 C++ 深度 + 30 分钟 Coding

| 时段 | 内容 |
|------|------|
| 0–15 min | 对象模型：Rule of Five、移动、RVO |
| 15–25 min | 智能指针 + 虚函数/vtable |
| 25–30 min | vector 扩容 或 mutex+CV |
| 30–50 min | LeetCode Medium 1 题 |
| 50–60 min | 系统设计 C++ 组件（Ring Buffer / LRU） |
