# 高通 Camera Application Engineer (CE / AE) Coding 面试全景复习指南

## 一、 考察定位与特点

高通 **Camera Application Engineer (CE / AE)** 的 Coding 考核具有非常鲜明的**“嵌入式 / 系统级 / C++ 底层”**特色。面试官通常是 Camera 驱动、HAL（CamX / Chi-CDK）或 ISP 系统架构背景的资深工程师。

* **编程语言：** 90% 以上考察 **C / C++**（建议熟练使用 C++11/14/17 标准）。
* **核心考察维度：**
  1. **多线程并发与同步**（Camera Pipeline 生产者-消费者模型、帧队列、死锁与竞争）。
  2. **内存管理与指针运算**（内存对齐、重叠拷贝、自定义分配器）。
  3. **位操作与寄存器级运算**（Sensor 驱动配置、寄存器掩码、Stride 对齐）。
  4. **图像与领域数据结构**（Ring Buffer、YUV/Raw 寻址、矩阵旋转、LRU Cache）。
  5. **C/C++ 底层机制**（`volatile`、内存屏障、智能指针、虚拟析构、对象生命周期）。

---

## 二、 核心模块与经典高频代码题

### 1. 多线程与并发编程（Camera Pipeline 核心）

Camera 数据流从 Sensor 捕获、ISP 处理到 App 预览是一组高并发流水线，线程安全队列和双缓冲是最基础且高频的考题。

#### (1) 线程安全阻塞队列（Thread-Safe Blocking Queue / Frame Queue）
* **考察点：** `std::mutex`、`std::condition_variable`、虚假唤醒（Spurious Wakeup）、RAII 资源保护。

```cpp
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue {
 public:
  explicit ThreadSafeQueue(size_t max_size = 10) : max_size_(max_size) {}

  // 生产者接口
  void Push(const T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    // 防止队列满时无限制占用内存，阻塞等待
    cond_not_full_.wait(lock, [this]() { return queue_.size() < max_size_; });
    queue_.push(item);
    cond_not_empty_.notify_one();
  }

  // 消费者接口
  bool Pop(T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    // 队列为空时等待生产者通知，避免虚假唤醒
    cond_not_empty_.wait(lock, [this]() { return !queue_.empty(); });
    item = queue_.front();
    queue_.pop();
    cond_not_full_.notify_one();
    return true;
  }

  bool IsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cond_not_empty_;
  std::condition_variable cond_not_full_;
  size_t max_size_;
};
