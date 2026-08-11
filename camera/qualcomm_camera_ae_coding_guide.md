# Qualcomm Camera Application Engineer (CE / AE) Coding Interview Guide
### Complete Systems, Concurrency, Memory & Image Pipeline Coding Handbook (Junior to Senior/Staff Level)

---

## 目录 (Table of Contents)
1. [考察定位与技术图谱 (Interview Scope & Technical Map)](#1-考察定位与技术图谱)
2. [模块一：并发与多线程 (Concurrency & Multi-Threading)](#2-模块一并发与多线程)
   - 2.1 基础题：线程安全阻塞队列 (Thread-Safe Blocking Queue)
   - 2.2 资深题：无锁单生产者单消费者队列 (Lock-Free SPSC Ring Buffer with Memory Order & False Sharing Prevention)
3. [模块二：内存管理与底层指针 (Memory & Pointer Management)](#3-模块二内存管理与底层指针)
   - 3.1 基础题：手写 `memmove` (处理 Memory Overlap)
   - 3.2 进阶题：硬件对齐内存分配器 `aligned_malloc` & `aligned_free` (64-byte / 128-byte DMA Alignment)
   - 3.3 资深题：Camera Buffer Pool (基于 `std::shared_ptr` 自定义 Deleter 的零拷贝引用管理)
4. [模块三：驱动级位运算与硬件接口 (Embedded C & Bitwise Operations)](#4-模块三驱动级位运算与硬件接口)
   - 4.1 大小端检测 (Endianness Check)
   - 4.2 寄存器位域操作 (Set/Clear/Toggle/Get Bit)
   - 4.3 步长与对齐计算 (Power-of-2 Alignment for Buffer Stride)
   - 4.4 二进制 1 计数与位反转 (Hamming Weight & Bit Reversal)
5. [模块四：Camera 领域算法与数据流 (Camera Domain Algorithms & Dataflow)](#5-模块四camera-领域算法与数据流)
   - 5.1 环形数据流缓冲区 (Circular Ring Buffer for Camera Frames)
   - 5.2 资深题：多摄像头纳秒级时间戳同步器 (Multi-Camera Timestamp Sync Matcher)
   - 5.3 图像原址顺时针 90 度旋转 (In-Place 2D Matrix Rotation)
   - 5.4 考虑 Stride 的 2D 卷积/滤波与边界处理 (2D Convolution with Line Buffers & Stride)
   - 5.5 YUV / NV12 / NV21 寻址与像素提取 (YUV420 Memory Layout & Addressing)
6. [模块五：C/C++ 语言核心八股与原理速查 (C++ Core Trivia & Deep Dive)](#6-模块五cc-语言核心八股与原理速查)
7. [面试实战高分策略与 Checklist (Senior Interview Checklist)](#7-面试实战高分策略与-checklist)

---

## 1. 考察定位与技术图谱

高通 **Camera Application Engineer (CE / AE)** 的 Coding 考核具有非常鲜明的**“嵌入式 / 系统级 / C++ 底层”**特色。面试官通常是 Camera 驱动、HAL（CamX / Chi-CDK）或 ISP 系统架构背景的资深工程师。

```
+-----------------------------------------------------------------------------------+
|               Qualcomm Camera AE / CE Coding Technical Pillars                    |
+-----------------------------------------------------------------------------------+
| 1. Concurrency & IPC      | Thread-safe Queue, SPSC Lock-free Ring Buffer,         |
|                           | Atomic Memory Order (Acquire-Release), Mutex/CondVar  |
+---------------------------+-------------------------------------------------------+
| 2. Memory & Zero-Copy     | DMA-BUF, Buffer Pool, Aligned Malloc, Cache Flush,    |
|                           | Custom Deleters, Memmove with Overlap Protection       |
+---------------------------+-------------------------------------------------------+
| 3. Hardware / Driver C    | Bitmask Manipulation, Stride Alignment, Endianness,  |
|                           | Register Bitfields, Volatile, Struct Packing (#pragma)|
+---------------------------+-------------------------------------------------------+
| 4. Camera Dataflow & Math | Frame Correlator (Time-Sync), YUV/Raw Addressing,     |
|                           | In-place Matrix Transpose, 2D Separable Filter        |
+-----------------------------------------------------------------------------------+
```

---

## 2. 模块一：并发与多线程 (Concurrency & Multi-Threading)

### 2.1 基础题：线程安全阻塞队列 (Thread-Safe Blocking Queue)
* **场景：** 生产者线程（Sensor Ingestion）将采集帧放入队列，消费者线程（ISP/Algorithm Processing）取出处理。
* **核心点：** `std::mutex`、`std::condition_variable`、双条件变量（`not_full` 和 `not_empty`）、防虚假唤醒。

```cpp
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue {
 public:
  explicit ThreadSafeQueue(size_t max_size = 10) : max_size_(max_size) {}

  void Push(const T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_not_full_.wait(lock, [this]() { return queue_.size() < max_size_; });
    queue_.push(item);
    cond_not_empty_.notify_one();
  }

  bool Pop(T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
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
```

---

### 2.2 资深题：无锁单生产者单消费者队列 (Lock-Free SPSC Ring Buffer)
* **场景：** 120 FPS / 4K 实时流中，Mutex 上下文切换耗时不可接受。要求在单 Producer 和单 Consumer 场景下实现**零锁开销、零动态分配、无 Cache Line 伪共享**的环形队列。
* **核心点：**
  * `Capacity` 约束为 $2^N$，取模优化为 `current_index & (Capacity - 1)`。
  * 显式指定内存顺序：`std::memory_order_relaxed`、`std::memory_order_acquire`、`std::memory_order_release`。
  * `alignas(64)` 隔离读写指针，防止多核 CPU **False Sharing**。

```cpp
#include <atomic>
#include <cstddef>
#include <new>

template <typename T, size_t Capacity>
class LockFreeSPSCQueue {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

 public:
  LockFreeSPSCQueue() : head_(0), tail_(0) {
    buffer_ = reinterpret_cast<T*>(new char[sizeof(T) * Capacity]);
  }

  ~LockFreeSPSCQueue() {
    T dummy;
    while (Pop(dummy)) {}
    delete[] reinterpret_cast<char*>(buffer_);
  }

  // 单生产者写入
  bool Push(const T& item) {
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t current_head = head_.load(std::memory_order_acquire);

    // 队列满判定
    if ((current_tail - current_head) == Capacity) {
      return false; // Buffer Full
    }

    new (&buffer_[current_tail & (Capacity - 1)]) T(item);
    // Release 屏障：保证数据构造完成后再更新 tail_
    tail_.store(current_tail + 1, std::memory_order_release);
    return true;
  }

  // 单消费者读取
  bool Pop(T& item) {
    const size_t current_head = head_.load(std::memory_order_relaxed);
    const size_t current_tail = tail_.load(std::memory_order_acquire);

    // 队列空判定
    if (current_head == current_tail) {
      return false; // Buffer Empty
    }

    size_t index = current_head & (Capacity - 1);
    item = buffer_[index];
    buffer_[index].~T();

    // Release 屏障：保证数据析构完成后再更新 head_
    head_.store(current_head + 1, std::memory_order_release);
    return true;
  }

 private:
  T* buffer_;

  // 关键：对齐到 64 字节缓存行，防止跨核 False Sharing
  alignas(64) std::atomic<size_t> tail_;
  alignas(64) std::atomic<size_t> head_;
};
```

---

## 3. 模块二：内存管理与底层指针 (Memory & Pointer Management)

### 3.1 基础题：手写 `memmove` (处理内存重叠 Memory Overlap)
* **核心点：**
  * 若目标地址 $d < s$ 或 $d \ge s + n$：正向拷贝（从前往后）。
  * 若 $s < d < s + n$（目标地址在源区间内部发生重叠）：反向拷贝（从后往前）。

```c
#include <stddef.h>

void* my_memmove(void* dest, const void* src, size_t n) {
  char* d = (char*)dest;
  const char* s = (const char*)src;

  if (d == s || n == 0) return dest;

  if (d < s || d >= s + n) {
    // 正向拷贝
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    // 重叠：反向拷贝
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dest;
}
```

---

### 3.2 进阶题：硬件对齐内存分配器 `aligned_malloc` & `aligned_free`
* **场景：** ISP / GPU / DSP 的 DMA 传输通常要求 64-byte 或 128-byte 物理地址对齐。
* **核心点：** 额外分配空间存储对齐偏移量与原始指针首地址。

```c
#include <stdint.h>
#include <stdlib.h>

void* aligned_malloc(size_t size, size_t alignment) {
  // 额外空间：size + alignment - 1 + 保存原始指针的 void* 大小
  void* raw_ptr = malloc(size + alignment - 1 + sizeof(void*));
  if (!raw_ptr) return NULL;

  uintptr_t raw_addr = (uintptr_t)raw_ptr + sizeof(void*);
  uintptr_t aligned_addr = (raw_addr + (alignment - 1)) & ~(alignment - 1);

  // 在对齐地址前方的指针位置存储 raw_ptr
  void** ptr_storage = (void**)aligned_addr;
  ptr_storage[-1] = raw_ptr;

  return (void*)aligned_addr;
}

void aligned_free(void* aligned_ptr) {
  if (!aligned_ptr) return;
  void** ptr_storage = (void**)aligned_ptr;
  free(ptr_storage[-1]);
}
```

---

### 3.3 资深题：Camera Buffer Pool (带自定义 Deleter 的引用计数管理)
* **场景：** 4K 图像帧（每帧数十 MB），严禁实时每帧 `malloc`/`free`。StreamOn 时预分配固定块 Buffer Pool，通过智能指针自定义 Deleter 实现下游各模块消费完毕后**自动归还内存池**。

```cpp
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

class FrameBuffer {
 public:
  explicit FrameBuffer(size_t size) : size_(size), data_(new uint8_t[size]) {}
  ~FrameBuffer() { delete[] data_; }
  uint8_t* Data() { return data_; }
  size_t Size() const { return size_; }

 private:
  size_t size_;
  uint8_t* data_;
};

class CameraBufferPool : public std::enable_shared_from_this<CameraBufferPool> {
 public:
  CameraBufferPool(size_t buffer_size, size_t pool_size) : buffer_size_(buffer_size) {
    for (size_t i = 0; i < pool_size; ++i) {
      free_buffers_.push_back(new FrameBuffer(buffer_size_));
    }
  }

  ~CameraBufferPool() {
    for (auto* buf : free_buffers_) {
      delete buf;
    }
  }

  // 外部获取 Buffer，使用自定义 Deleter
  std::shared_ptr<FrameBuffer> AcquireBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (free_buffers_.empty()) {
      return nullptr; // 资源耗尽，触发丢帧策略
    }

    FrameBuffer* raw_buf = free_buffers_.back();
    free_buffers_.pop_back();

    auto weak_self = std::weak_ptr<CameraBufferPool>(shared_from_this());

    // 当下游最后一个 shared_ptr 析构时，不 delete 内存，而是回调归还池子
    return std::shared_ptr<FrameBuffer>(raw_buf, [weak_self](FrameBuffer* ptr) {
      if (auto self = weak_self.lock()) {
        self->RecycleBuffer(ptr);
      } else {
        delete ptr;
      }
    });
  }

 private:
  void RecycleBuffer(FrameBuffer* buf) {
    std::lock_guard<std::mutex> lock(mutex_);
    free_buffers_.push_back(buf);
  }

  size_t buffer_size_;
  std::mutex mutex_;
  std::vector<FrameBuffer*> free_buffers_;
};
```

---

## 4. 模块三：驱动级位运算与硬件接口 (Embedded C & Bitwise Operations)

```c
#include <stdbool.h>
#include <stdint.h>

// 1. 判断大小端（Endianness）
bool is_little_endian(void) {
  uint16_t test = 0x0001;
  return (*((uint8_t*)&test) == 0x01);
}

// 2. 寄存器位操作宏定义
#define SET_BIT(reg, bit)    ((reg) |= (1U << (bit)))
#define CLEAR_BIT(reg, bit)  ((reg) &= ~(1U << (bit)))
#define TOGGLE_BIT(reg, bit) ((reg) ^= (1U << (bit)))
#define GET_BIT(reg, bit)    (((reg) >> (bit)) & 1U)

// 3. 向上对齐到 2 的整数次幂（常用于计算硬件 Buffer Stride）
#define ALIGN_UP(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))

// 4. 二进制中 1 的个数（Hamming Weight）
int count_set_bits(uint32_t n) {
  int count = 0;
  while (n) {
    n &= (n - 1); // 清除最低位的 1
    count++;
  }
  return count;
}

// 5. 32 位整数二进制位反转（用于 Sensor 镜像/翻转解包）
uint32_t reverse_bits(uint32_t n) {
  n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);
  n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);
  n = ((n >> 4) & 0x0F0F0F0F) | ((n & 0x0F0F0F0F) << 4);
  n = ((n >> 8) & 0x00FF00FF) | ((n & 0x00FF00FF) << 8);
  n = (n >> 16) | (n << 16);
  return n;
}
```

---

## 5. 模块四：Camera 领域算法与数据流 (Camera Domain Algorithms & Dataflow)

### 5.1 环形缓冲区 (Circular Ring Buffer for Camera Frames)
* **场景：** 零延时快门（ZSL）保留最近 $N$ 帧图像，满时自动覆写最旧帧。

```cpp
#include <vector>

template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity)
      : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), size_(0) {}

  void Push(const T& item) {
    buffer_[head_] = item;
    head_ = (head_ + 1) % capacity_;
    if (size_ < capacity_) {
      size_++;
    } else {
      // 队列已满，覆盖旧数据，tail 指针后移
      tail_ = (tail_ + 1) % capacity_;
    }
  }

  bool Pop(T& item) {
    if (IsEmpty()) return false;
    item = buffer_[tail_];
    tail_ = (tail_ + 1) % capacity_;
    size_--;
    return true;
  }

  bool IsEmpty() const { return size_ == 0; }
  bool IsFull() const { return size_ == capacity_; }
  size_t Size() const { return size_; }

 private:
  std::vector<T> buffer_;
  size_t capacity_;
  size_t head_;
  size_t tail_;
  size_t size_;
};
```

---

### 5.2 资深题：多摄像头时间戳同步匹配器 (Multi-Camera Timestamp Sync Matcher)
* **场景：** 双目深度/人像虚化（Wide + Tele）。两路相机存在独立硬件时钟抖动和不同曝光时间，需在容忍窗口 $\Delta T$（如 15ms）内对齐帧并淘汰孤立过期帧。

```cpp
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

struct CameraFrame {
  int64_t timestamp_ns;
  int camera_id;
  void* buffer_handle;
};

class DualCameraSync {
 public:
  explicit DualCameraSync(int64_t max_tolerance_ns = 15'000'000)
      : max_tolerance_ns_(max_tolerance_ns) {}

  void PushFrame(const CameraFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame.camera_id == 0) {
      cam0_queue_.push_back(frame);
    } else {
      cam1_queue_.push_back(frame);
    }
  }

  std::optional<std::pair<CameraFrame, CameraFrame>> GetMatchedPair() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!cam0_queue_.empty() && !cam1_queue_.empty()) {
      int64_t t0 = cam0_queue_.front().timestamp_ns;
      int64_t t1 = cam1_queue_.front().timestamp_ns;
      int64_t diff = t0 - t1;

      // 匹配成功：时间差在容限内
      if (std::abs(diff) <= max_tolerance_ns_) {
        CameraFrame f0 = cam0_queue_.front();
        CameraFrame f1 = cam1_queue_.front();
        cam0_queue_.pop_front();
        cam1_queue_.pop_front();
        return std::make_pair(f0, f1);
      }

      // cam0 落后过多，说明其配对帧已丢失，丢弃 cam0 首帧
      if (t0 < t1) {
        cam0_queue_.pop_front();
      } else {
        // cam1 落后过多，丢弃 cam1 首帧
        cam1_queue_.pop_front();
      }
    }

    return std::nullopt;
  }

 private:
  std::mutex mutex_;
  int64_t max_tolerance_ns_;
  std::deque<CameraFrame> cam0_queue_;
  std::deque<CameraFrame> cam1_queue_;
};
```

---

### 5.3 图像原址顺时针旋转 90 度 (In-Place Rotate Image)
* **思路：** 矩阵转置（Transpose） $\rightarrow$ 逐行翻转（Reverse Rows）。时间复杂度 $O(N^2)$，空间复杂度 $O(1)$。

```cpp
#include <algorithm>
#include <vector>

void RotateImage90Clockwise(std::vector<std::vector<int>>& matrix) {
  int n = matrix.size();
  if (n <= 1) return;

  // 1. 转置矩阵
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      std::swap(matrix[i][j], matrix[j][i]);
    }
  }

  // 2. 翻转每一行
  for (int i = 0; i < n; i++) {
    std::reverse(matrix[i].begin(), matrix[i].end());
  }
}
```

---

### 5.4 考虑 Stride 的 2D 卷积/滤波与边界处理 (2D Convolution with Line Buffers & Stride)
* **场景：** 考查图像在物理内存中的连续性（Stride vs. Width）、Cache Locality（行优先遍历）与边缘填充处理。

```cpp
#include <algorithm>
#include <cstdint>

void BoxBlur3x3(const uint8_t* src, uint8_t* dst, int width, int height, int stride) {
  if (!src || !dst || width <= 0 || height <= 0 || stride < width) return;

  for (int y = 0; y < height; ++y) {
    int y_min = std::max(0, y - 1);
    int y_max = std::min(height - 1, y + 1);

    for (int x = 0; x < width; ++x) {
      int x_min = std::max(0, x - 1);
      int x_max = std::min(width - 1, x + 1);

      int sum = 0;
      int count = 0;

      // 严格利用 Stride 进行连续行优先内存寻址
      for (int ny = y_min; ny <= y_max; ++ny) {
        const uint8_t* row_ptr = src + ny * stride;
        for (int nx = x_min; nx <= x_max; ++nx) {
          sum += row_ptr[nx];
          count++;
        }
      }

      dst[y * stride + x] = static_cast<uint8_t>(sum / count);
    }
  }
}
```

---

### 5.5 YUV / NV12 / NV21 内存布局与像素寻址公式

```
NV12 (Semi-Planar):
+-----------------------------+
| Y0  Y1  Y2  Y3  ... (Width) |  Height (Y-Plane)
| Y4  Y5  Y6  Y7  ...         |
+-----------------------------+
| U0  V0  U1  V1  ... (Width) |  Height / 2 (UV-Interleaved Plane)
+-----------------------------+
```

* **大小计算：**
  * $Y$ 平面大小 $= \text{Stride} \times \text{Height}$
  * $UV$ 平面大小 $= \text{Stride} \times (\text{Height} / 2)$
  * 总字节数 $= \text{Stride} \times \text{Height} \times 1.5$
* **寻址公式（给定像素坐标 $(x, y)$）：**
  * $Y$ 偏移量 $= y \times \text{Stride} + x$
  * $UV$ 平面起始基址 $= \text{Height} \times \text{Stride}$
  * $U$ 偏移量（NV12）$= \text{UV\_Base} + (y / 2) \times \text{Stride} + (x / 2) \times 2$
  * $V$ 偏移量（NV12）$= U\text{\_Offset} + 1$

---

## 6. 模块五：C/C++ 语言核心八股与原理速查

| 核心概念 | 面试高频问题与答案要点 |
| :--- | :--- |
| **`volatile`** | **不能保证线程安全。** 它仅告诉编译器不要对该变量做寄存器缓存优化，每次读写必须访问真实内存地址。常用于 MMIO 硬件寄存器映射或中断标志。**不提供原子性，也不包含内存屏障**。 |
| **`std::atomic` & 内存顺序** | `memory_order_seq_cst`（默认全局严格全序，开销最大）；`memory_order_relaxed`（仅保证原子读写，无指令重排限制）；`acquire-release`（配对屏障：保证 release 前的写操作对后续 acquire 线程完全可见）。 |
| **智能指针与循环引用** | `unique_ptr`（独占所有权、零空间/时间额外开销）；`shared_ptr`（控制块包含强引用计数与弱引用计数）；循环引用（如 Observer 模式）使用 `weak_ptr` 解环，通过 `lock()` 升级为 `shared_ptr` 安全访问。 |
| **虚析构函数 (Virtual Destructor)** | 当通过基类指针 `delete` 派生类对象时，如果基类析构函数不是 `virtual`，只会调用基类析构函数而不会调用派生类析构函数，造成派生类成员资源泄漏。 |
| **结构体对齐与 `#pragma pack`** | 默认遵循最大成员对齐原则。`#pragma pack(1)` 强制 1 字节紧凑排列，用于跨芯片/跨网络通信协议包，但可能导致 CPU 产生非对齐访存（Unintuitive fault 或性能惩罚）。 |
| **RAII 机制** | Resource Acquisition Is Initialization。将资源生命周期绑定至栈对象，利用析构函数自动释放锁、内存、DMA 句柄，保证在发生异常或提前 `return` 时资源绝不泄漏。 |

---

## 7. 面试实战高分策略与 Checklist

1. **审题与澄清需求 (Clarification)：**
   * 确认数据规模与物理边界（图像 Width、Height、Stride、空指针）。
   * 确认线程模型（单线程 vs 多线程、SPSC vs MPMC）。
   * 确认性能与内存约束（是否允许开辟额外堆内存？是否要求零拷贝？）。
2. **代码鲁棒性与边界处理 (Defensive Coding)：**
   * 函数开头第一时间对入参指针判空（`if (!ptr) return;`）。
   * 循环防越界与负数保护。
   * 锁与动态内存严格配对（优先采用 RAII 容器和智能指针）。
3. **边写边讲 (Think Aloud & Proactive Deepening)：**
   * 先用 1 分钟阐述整体算法思路。
   * 主动提及系统级考量（如：Cache Locality 行优先遍历、避免 False Sharing、DMA 对齐等）。
