# 场景 1：AR/VR 空间感知 — 多相机 6DoF SLAM / Controller Tracking

**典型题：** 设计一个针对 6DoF SLAM / Controller Tracking 的多相机同步感知系统。

**核心考点：** 多 Sensor FSYNC/VSYNC、Global vs Rolling Shutter、曝光锁相、IMU–Camera 时间戳、MIPI 低功耗高帧率路径。

---

## 1. 需求与约束

先钉死产品，再画图。VR 头显与眼镜 tracking 相机不是同一套预算。

| 维度 | 默认假设（头显 tracking） | 为什么 |
|------|---------------------------|--------|
| 相机 | 4 路 inward/world GS（两对立体）+ 可选 2 路控制器 IR | 6DoF 需要多视角；控制器常用闪烁 LED + 头显相机 |
| 规格 | 640×480 或 800×600，**RAW8/10**，60–90 fps（冲刺 120） | 特征点够用；分辨率上去带宽和 ISP 立刻爆 |
| IMU | 6 轴 1 kHz，与相机 **同一晶振域或可测偏移** | VIO 融合；1ms 误差在 100°/s 转动下是像素级错位 |
| 延迟 | 曝光中点 → pose 输出 **< 5–10 ms** | 合成层 / 重投影要赶上下一帧 display |
| 功耗 | tracking 子系统持续 **< 几百 mW** 量级（头显可略宽） | 不能走完整拍照 ISP |
| 失败 | 丢 FSYNC、MIPI CRC、SOF timeout、热降帧 | IMU 不能停；要有 degrade 而不是黑屏 |

**开场金句：** 这不是拍照 Pipeline。Tracking 相机要 **固定或缓变曝光、Global Shutter、硬件同步、FE-only、零拷贝进 CV/DSP**。美颜、TNR、3A 猎焦全部砍掉。

---

## 2. 高层架构

```
                    SoC 时钟 / 同步器
                           │ FSYNC (GPIO / dedicated PLL)
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
     [GS Cam L]       [GS Cam R]     [GS Cam …]     [IMU 1kHz]
           │               │               │              │
           │ MIPI CSI-2    │               │              │ SPI/I2C/SLIMbus
           ▼               ▼               ▼              ▼
        CSIPHY/CSID  (每口独立或 VC 复用)
           │
           ▼
        IFE FE-only：BLC + 可选 binning + 硬件时间戳
           │ 不解 Bayer 成预览 YUV
           ▼
        dma-buf 环（每路 SPSC，cache-line 对齐）
           │ fence
           ▼
        Tracking DSP / NPU：特征 → 光流/描述子
           │
           ▼
        VIO / SLAM 融合（IMU 预积分 + 视觉更新）
           │
           ▼
        Pose 给 compositor / controller 解算
```

**控制面：** CCI 配 mode、exposure、gain、FSYNC slave。  
**数据面：** MIPI → CSID → IFE DMA，CPU 不碰像素。

与拍照 ISP 解耦：同一颗 SoC 上 **CV usecase graph ≠ media usecase graph**。详见 [`02-e2e-isp-pipeline.md`](./02-e2e-isp-pipeline.md)。

---

## 3. 核心子系统

### 3.1 多 Sensor 硬件同步（FSYNC / VSYNC）

软件“尽量同一时刻 `streamon`”不够。90fps 下一帧 11ms，1ms 的软件抖动就是 **8% 的帧相位误差**，立体匹配和多目三角直接漂。

推荐拓扑：

1. **SoC 主时钟** 产生周期 FSYNC 脉冲（或专用 sync PLL）。
2. 所有 tracking sensor 配成 **external trigger / slave**：收到 FSYNC 边沿才开始积分。
3. Master 不再用“某颗 sensor 当主、其余跟”——主从级联会叠 settle 延迟。统一 SoC 触发，相位可配。

Bring-up 验证（面试加分）：示波器看各路 `FSYNC` 与 `FSIN` 边沿，目标 **< 50–100 µs** 对齐（具体看 sensor 手册）。再对比各路 SOF 硬件时间戳差。

VSYNC 是显示域概念。Tracking 同步说 **FSYNC / FSIN**；若还要和显示锁相（passthrough），另做 **display genlock**，不要和相机 FSYNC 混成一个词。

### 3.2 Global Shutter vs Rolling Shutter

| | Global Shutter | Rolling Shutter |
|---|---|---|
| 积分 | 全阵列同时 | 逐行开始 |
| 快转头 / 摆手 | 特征几何真 | 垂线变斜、控制器 LED 拖影 |
| 噪声 / 成本 / 功耗 | 通常更差、更贵 | 手机主摄主流 |
| Tracking | **必选** | 仅低端或极低速兜底 |

口述：**Rolling shutter 行时间 × 角速度 = 像素剪切**。IMU 可以补偿 RS，但要精确到行时间戳，标定和算力都更贵。头显 tracking 用 GS 把问题从算法挪回硬件，这是正确的钱。

主摄 / passthrough 仍可能是 RS：那条路径走 EIS + 行时间戳，**不要和 tracking 相机共用同一套曝光策略**。

### 3.3 曝光动态锁相

Tracking 要在“不糊”和“不欠曝”之间：

- **运动优先：** 曝光上限钳位（例如 ≤ 2–4 ms @ 90fps），宁可提 analog/digital gain。
- **多路同一曝光时间：** 立体对必须相同 integration time，否则亮度不一致，匹配打穿。Gain 可以每路略调（镜头、遮光不同）。
- **不要跑完整 AE 猎值：** 用慢环（几十帧）或分区增益；禁止 3A 每帧大幅改曝光导致光流失效。
- **室内闪烁：** 50/60Hz 灯。GS 整帧同时积分，选曝光为电网周期整数倍可减 banding；RS 更惨。检测电网频率后锁 exposure。

“锁相”在这里有两层：**(a) 多路曝光时间一致；(b) 与市电/PWM 光源相位稳定。** 面试时分开说。

### 3.4 IMU–Camera 时间同步

VIO 误差对时间比对内参更敏感。目标：每帧一个 **曝光中点时间戳**，落在 **IMU 时钟域**。

```
Sensor SOF ──► CSI/IFE 打硬件 timestamp（SoC 单调钟或专用 timer）
IMU sample ──► 同一 timer 或已知换算的 PHC

t_mid = t_SOF + t_exposure / 2     （GS）
t_mid_row = t_SOF + row * t_line   （若被迫用 RS）

pose 融合使用 t_mid，而不是“用户态 clock_gettime 收到 buffer 的时间”
```

校准：

1. **静态偏移：** 产线或开机用 LED / 机械激励同时打 IMU 和相机，估 $\Delta t_0$。
2. **漂移：** 两路时钟不同晶振，用缓慢 Kalman / PLL 跟 $\dot{\Delta t}$（典型 ppm 级）。
3. **禁止** 用“buffer 出队时间”当曝光时间——那是调度延迟。

Controller tracking：头显相机看到 LED 闪烁码，LED 与控制器 IMU 也要同步。常用 **光编码 + 同一无线时钟域**，或头显发 IR sync。讲清楚 **三方时间**（头显 IMU、头显相机、手柄 IMU）。

### 3.5 MIPI CSI-2 → SoC/DSP 传输

带宽粗算（RAW8，640×480@90fps，4 路）：

$$
640 \times 480 \times 1 \times 90 \times 4 \approx 110\ \mathrm{MB/s}
$$

加上 MIPI 包头、RAW10（×1.25）、blanking，仍远小于 4K 视频。瓶颈通常是 **口数 / lane 数 / 同时 IFE 客户端**，不是理论 Gbps。

要点：

- 每路独立 CSI 口最干净；口不够再用 **VC 复用**（CSID 按 VC 分给不同 IFE）。
- Tracking 走 **RAW8/10 packed**，在 CV 侧解包或 FE 解成 8-bit。不要升到 16-bit preview。
- 连续 dma-buf 环，深度 3–4：sensor 写 n、CV 读 n-1，多一帧抗 ISR 抖动。
- 低功耗：lane 在帧间隙进 LP 模式；不要为 tracking 开 C-PHY 全家桶除非口不够。

CSIPHY vs CSID 故障切分见 [`camera/sensor.md`](../../camera/sensor.md)。

---

## 4. 性能优化与边界

| 问题 | 策略 |
|------|------|
| 延迟 | FE-only；特征提取在 DSP；pose 用上一帧 IMU 传播（async）赶显示 |
| 带宽 | binning / ROI；室内降到 60fps；远处相机 30fps（若算法允许） |
| 掉帧 | IMU 继续预积分；视觉更新用“上一有效帧 + 更大协方差”；**不要**插一张黑帧进 SLAM |
| 热 | 先降 CV 复杂度（描述子维度）再降 fps；FSYNC 周期跟着改，避免“相机 45fps、算法仍按 90 等帧” |
| 失同步 | 检测 SOF 时间戳差超阈值 → 丢该立体对、单目+IMU 降级；告警校准 |

Tiling：tracking 分辨率小，一般不必 stripe；passthrough 才 stripe。

---

## 5. 跨团队落地

| 团队 | 你要对齐的契约 |
|------|----------------|
| Sensor HW | FSIN 极性/建立时间、GS 行读出、OTP 坏点、每路 extrinsics 安装公差 |
| 结构 / 光学 | 基线、遮光、IR 滤光；热膨胀 → 外参随温度的一阶模型 |
| 算法 / SLAM | 时间戳定义（SOF vs 曝光中点）、坐标系、丢帧语义、最大曝光 |
| IQ | tracking 几乎无 IQ；只保证 BLC 稳定、不饱和。不要把拍照 LSC 套过来导致特征漂移 |
| FW / 驱动 | FSYNC PLL、SOF 打戳寄存器、dma-buf 队列深度 |

产线：Zhang 标定内参 + 立体外参；IMU-cam 用视觉靶或 LED。结果进 **校准 blob**，与 tracking binary 版本绑定（同 OTA 故事，见总清单 OTA 节）。

**收口金句：** 多相机 tracking 的正确性首先是 **同一微秒开始曝光 + 同一时钟域的曝光中点时间戳**；算法再强也补不了未定义的时间。
