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
    // 保存所有 worker 线程。析构时必须逐个 join，保证线程退出后
    // ThreadPool 的成员（尤其 tasks_）才被销毁，避免 use-after-free。
    std::vector<std::thread> workers_;

    // 多生产者、多消费者任务队列：
    // - submit() 是生产者；
    // - worker 是消费者；
    // - std::function<void()> 用类型擦除统一保存不同类型的可调用对象。
    // 容量有限，可以通过阻塞 submit() 形成背压，防止任务无限堆积。
    BoundedBlockingQueue<std::function<void()>> tasks_;

    // 多个 worker 与析构线程并发访问停止标志，因此必须使用 atomic，
    // 不能使用普通 bool，否则会产生 data race（未定义行为）。
    std::atomic<bool> stop_{false};

public:
    // 先构造容量为 1024 的任务队列，再创建 n 个 worker。
    explicit ThreadPool(size_t n) : tasks_(1024) {
        for (size_t i = 0; i < n; ++i) {
            // emplace_back 直接在 vector 中构造 std::thread。
            // lambda 捕获 this，使 worker 可以访问 stop_ 和 tasks_；
            // 因此 ThreadPool 的生命周期必须长于所有 worker。
            workers_.emplace_back([this] {
                // worker 循环检查停止标志。atomic 默认使用
                // memory_order_seq_cst，能够安全地看到析构线程的写入。
                while (!stop_) {
                    // try_get() 是非阻塞获取：
                    // - 取到任务：optional 有值，调用 operator() 执行任务；
                    // - 队列为空：立即返回空 optional，然后继续循环。
                    // 注意：这个简化版会 busy-spin，空闲时仍持续消耗 CPU。
                    if (auto task = tasks_.try_get()) (*task)();
                }
            });
        }
    }

    template <typename F>
    void submit(F&& f) {
        // 转发左值/右值，减少不必要的可调用对象拷贝。
        // put() 在队列已满时阻塞，从而为提交方提供背压。
        tasks_.put(std::forward<F>(f));
    }

    ~ThreadPool() {
        // 通知所有 worker 停止。这个骨架采用“立即停止”语义：
        // stop_ 变为 true 后，队列中尚未执行的任务可能被丢弃。
        stop_ = true;

        // join 等待线程真正退出。若不 join，std::thread 在仍 joinable
        // 的状态下析构会调用 std::terminate()。
        for (auto& t : workers_) {
            t.join();
        }
    }
};
```

**口述：** 任务队列解耦提交与执行；`stop` 用 atomic；析构 join 所有线程；任务类型用 `std::function` 或类型擦除。

> **生产级注意：** 这是便于白板书写的骨架，不是完整实现。`try_get()` 会导致空闲 worker 忙等；更合理的实现是让 worker 阻塞在条件变量上，并在关闭时唤醒全部线程。还要明确关闭语义（立即丢弃还是 drain 完剩余任务）、拒绝 shutdown 后的 `submit()`，并处理任务异常，防止异常逃出线程入口触发 `std::terminate()`。

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
