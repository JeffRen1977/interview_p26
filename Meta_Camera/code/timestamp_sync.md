# 相机–IMU 时间戳对齐

**这题你的清单上原本没有，但 Meta tracking 方向大概率会问。**「多相机 + IMU 怎么对时」是 6DoF/VIO 的第一性问题，而它有一个非常清爽的 coding 形式：两条有序流的归并、二分、插值、时钟偏移估计。

实现与自测：[`timestamp_sync.cpp`](./timestamp_sync.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 timestamp_sync.cpp -o timestamp_sync && ./timestamp_sync
```

---

## 1. 第一句话：一帧不是一个时刻

融合要用的是**曝光中点**，不是 SOF，也不是 EOF：

$$
t_{\text{mid}} = t_{\text{SOF}} + \frac{t_{\text{exposure}}}{2}
$$

**Rolling shutter 还要加行延迟**——第 $y$ 行比第 0 行晚曝光：

$$
t_{\text{mid}}(y) = t_{\text{SOF}} + \frac{t_{\text{readout}} \cdot y}{H-1} + \frac{t_{\text{exposure}}}{2}
$$

Global shutter 时 `readout = 0`，所有行同一时刻——这就是 tracking 相机一定用 GS 的原因：RS 下每一行对应不同的 pose，VIO 要么建模要么误差直接吃进去。

**时间戳一律用 `int64_t` 纳秒。** 不要用 `double`：53 位尾数，开机几周后 boot-relative ns 就吃掉大半精度，亚毫秒对齐直接失效。

## 2. 二分找区间 + 线性插值

```c
lower_bracket(imu, t)   // 最后一个 t_ns <= t 的下标，越界返回 -1
```

写 `mid = lo + (hi - lo) / 2` 而不是 `(lo + hi) / 2`——面试官会看这一行。

插值的关键是**拒绝外推**：查询点落在 buffer 之外，或者相邻两个 IMU 样本间隔超过阈值（掉了一批 IMU），必须返回失败让上层丢帧，而不是伪造一个陀螺读数。假数据进了 VIO 比丢一帧危险得多。

姿态（四元数）不能线性插值，要 **SLERP**；陀螺角速度可以线性插。面试时点一句。

## 3. 时钟偏移估计：取最小值，不是取平均

相机硬件时间戳在 sensor/CSI 时钟域，IMU 在 SoC 时钟域。你观测到的每一对 $(t_{\text{local}}, t_{\text{remote}})$ 都被一个**非负且抖动**的传输延迟污染：

$$
t_{\text{local}} = t_{\text{remote}} + \text{offset} + \text{delay},\quad \text{delay} \ge 0
$$

所以窗口内 **最小** 的差值是最好的估计（延迟最小的那次观测最干净）。**取平均是错的**——那是把排队噪声平均进去。窗口的 min–max 跨度就是可信度指标（jitter）。

这和 NTP / PTP 的思路一致，面试时可以这么类比。长期还要估**频率漂移**（两个晶振 ppm 不同），做法是对 offset 序列拟合一条线，斜率就是 skew。

## 4. 丢帧检测：两个独立信号

| 信号 | 来源 | 失效场景 |
|------|------|----------|
| 帧计数器 | sensor 自带 seq | mode switch / stream restart 会清零 |
| 时间戳间隔 | `dt / nominal_period` | 热降帧时周期本身变了 |

两个都算，**不一致时报警**——这正是抓「HAL 层重排序」「CSI 丢包但 driver 没发现」的钩子。计数器用 `uint32_t` 无符号减法，回绕是良定义的，别写成 `int` 然后报告丢了四十亿帧。

## 5. 追问清单

- 多相机怎么同步：**FSYNC 主从**（一颗 master 出 XVS，其余 slave 跟），而不是软件对齐。软件只能补残差。
- 硬件时间戳打在哪：理想是 **SOF 中断在 CSID 里打**，最差是 userspace 收到 buffer 才取 `CLOCK_MONOTONIC`——后者含调度抖动，几毫秒起。
- `CLOCK_MONOTONIC` vs `CLOCK_BOOTTIME`：suspend 期间前者停走。Android 相机 HAL 规定用 `CLOCK_BOOTTIME` 对齐 sensor HAL。
- 温度漂移：晶振 ppm 随温度变，长时间 session 要在线重估 offset，不能只在启动时标一次。
