# Meta Embedded Coding 面试指南

根据 Meta Embedded 面试官方指南整理。可运行 C++ 题在 [`code/`](./code/)；本页是节奏、规则和专题地图。总备考清单见 [`camera_software_engineer_prep.md`](./camera_software_engineer_prep.md)。

---

## 时间与规则

| | |
|---|---|
| 场次 | 60 分钟技术电面 |
| 前 15–20 分钟 | Technical Leadership / STAR 行为 |
| 后 40–45 分钟 | Coding，目标 **约 35 分钟内准确写出 2 道题** |
| 语言 | **必须 C 或 C++**（不要 Python） |
| 红线 | 面试中禁止未授权外部工具、ChatGPT 等 AI |

35 分钟两题意味着每题大约 **15–18 分钟**：先签名和边界（2 min）→ 正确的标量实现（8–10 min）→ 1–2 个追问（SIMD / 对齐 / 重叠 / 超时）。不要一上来写 NEON。

---

## 专题地图

### 1. 位运算与二进制（Bit Manipulation & Bytes）

**高频：** 位图、大小端转换 / 检测、掩码、popcount、按位打包解包（RAW10 / RAW12 → RAW16，RGB565 解包）。

| 实战 | 状态 | 文件 |
|------|------|------|
| 10-bit packed RAW Bayer → 16-bit 线性数组 | 已有 | [`code/mipi_raw10_unpack.md`](./code/mipi_raw10_unpack.md) |
| 32-bit 整型 endianness 检测与转换 | 已有 | [`code/endianness.md`](./code/endianness.md) |
| 位级环形移位 ROTL / ROTR | 已有 | [`code/bit_rotation.md`](./code/bit_rotation.md) |

白板口令：`k &= 31` 再移位，避免 `>> 32` UB；RAW10 是 4 像素 / 5 字节，输出 LSB 对齐 `[0,1023]`。

### 2. 底层系统与内存（Systems & Memory Management）

**高频：** 内存对齐、定长块分配器 / 内存池、零拷贝环形队列。

| 实战 | 状态 | 文件 |
|------|------|------|
| `aligned_malloc` / `aligned_free` | 已有 | [`code/aligned_malloc.md`](./code/aligned_malloc.md) |
| 图像帧 buffer 的无锁 SPSC 环 | 已有 | [`concurrency/spsc_ring_buffer.cpp`](../concurrency/spsc_ring_buffer.cpp)、[`XR/Pico_vision/24-无锁SPSC队列与Cacheline对齐.md`](../XR/Pico_vision/24-无锁SPSC队列与Cacheline对齐.md) |
| `memcpy` / `memmove`（处理 overlap） | 已有 | [`code/memmove.md`](./code/memmove.md) |

白板口令：对齐地址前一格藏 `raw_ptr`；`memmove` 先比较指针再决定拷贝方向；SPSC 用 acquire/release + `alignas(64)` 防 false sharing。

### 3. 并发与硬件交互（Concurrency & Embedded Fundamentals）

**高频：** 中断顶半部 / 底半部、mutex、条件变量、CAS、多线程帧同步。

| 实战 | 状态 | 文件 |
|------|------|------|
| 线程安全阻塞队列（带 timeout） | 已有 | [`concurrency/bounded_blocking_queue.cpp`](../concurrency/bounded_blocking_queue.cpp) |
| 读写锁或限流器 | 限流器示例 | [`AI_native_coding/ratelimiter_engine/`](../AI_native_coding/ratelimiter_engine/) |

白板口令：ISR 里只做最小工作（置标志 / 入无锁队列），处理放到 thread；`condition_variable::wait_for` 处理虚假唤醒； capturer→ISP 用 SPSC，多消费者才上 MPMC / 阻塞队列。

### 4. 图像基础与矩阵（2D Array / Image Basics）

**高频：** 2D 卷积 / Box filter、旋转与翻转、Bayer 双线性 demosaic、直方图 / 积分图、滑动窗口。

| 实战 | 状态 | 文件 |
|------|------|------|
| 3×3 均值 / 高斯滤波（padding） | 已有 | [`code/box_filter.md`](./code/box_filter.md) |
| 灰度直方图 + CDF（直方图均衡） | 已有 | [`code/histogram.md`](./code/histogram.md) |
| Bayer bilinear demosaic | 相近 | [`interview_handwrite/bilinear_interp.py`](../interview_handwrite/bilinear_interp.py) |

直方图均衡（8-bit）：

$$
\mathrm{cdf}[i] = \sum_{k=0}^{i} h[k],\quad
\mathrm{lut}[i] = \mathrm{round}\frac{(\mathrm{cdf}[i]-\mathrm{cdf}_{\min}) \times 255}{N - \mathrm{cdf}_{\min}}
$$

卷积不要漏 **stride / 边界**：`src + r * src_stride`，clamp 或 replicate padding，不要假设 `width * bpp` 就是行长。

---

## 冷练顺序（按官方专题）

先把 [`code/`](./code/) 里的题闭卷写完：

1. RAW10 unpack → `rotl32` → endian swap  
2. `aligned_malloc` → `memmove` → SPSC  
3. bounded blocking queue（timeout）  
4. 3×3 box filter with stride → histogram + CDF  

语言全程 C/C++。每题计时 18 分钟：能跑的正确性优先于 SIMD。
