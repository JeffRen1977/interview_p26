# 场景 2：End-to-End ISP — 拍照与视频流管道

**典型题：** 设计一个端到端手机 / 眼镜拍摄（拍照 + 视频）ISP 管道架构。

**核心考点：** RAW 域处理、3A 闭环、YUV/RGB 后处理、V4L2 与 Android HAL3 / CamX 解耦。

眼镜上仍可能有“给用户看的照片/视频”，但必须和 tracking 管道分开。本篇默认 **媒体路径**（quality > 亚毫秒级 pose）。

---

## 1. 需求与约束

| 维度 | 手机默认 | 眼镜默认 |
|------|----------|----------|
| 传感器 | 1–3 主/超广/长焦，RS，10–50MP | 1 颗小型 RS 主摄，12MP 级 |
| 预览 | 1080p30/60，玻璃到玻璃 **< 30 ms** 量级 | 更严：功耗允许才 30fps |
| 录像 | 4K30 / 1080p60，音画同步 | 1080p30，热墙随时降档 |
| 拍照 | ZSL / 多帧 HDR / 夜景 1–5s | 单帧或短 burst；不能让镜腿发烫 |
| 软件栈 | Camera2/HAL3 + OEM HAL（CamX/Chi 等） | 可能是裁剪 HAL 或专用 RTOS+Linux |

**开场金句：** 画两条并发流——**Preview/Video 实时 HW ISP** 与 **Still 高质量（可多帧、可进 NPU）**。3A 只吃 FE **Stats**，不吃最终美颜 YUV。

---

## 2. 高层架构

```
App / CameraX
        │ Binder CaptureRequest (settings + surfaces)
        ▼
CameraService / HAL3 session
        │ processCaptureRequest / Result + fences
        ▼
CamX / Chi graph：按 usecase 连 Node
        │ V4L2 ioctl + dma-buf fd
        ▼
Kernel: CCI 配 Sensor；CSIPHY/CSID 收 MIPI
        │
        ▼
IFE (Front-End)
        ├─ 像素：BLC → LSC → BPC → （可选 Bayer NR）
        ├─ Stats：BG / BHIST / BF → DSP 3A
        └─ 输出：线性 RAW 或已 demosaic 的中间域  →  dma-buf
                │
        ┌───────┴────────────┐
        ▼                    ▼
   Preview/Video          Still / Snapshot
   IPE：CCM, Gamma,       BPS 全尺寸 + 多帧对齐
   TNR 轻量, Sharpen,     MFNR/HDR 融合
   Scale 到 1080p         （可选 NPU RAW denoise）
        │                    │
        ▼                    ▼
   Encoder / Display      JPEG/HEIF + 全尺寸 YUV
```

数据流：**Sensor → MIPI CSI-2 → Driver → HW ISP → DMA-BUF → App/AI**。  
控制流：HAL3 Request 变成 sensor exposure + ISP IQ packet，Result 带回 timestamp / 3A 元数据。

模块职责见 [`camera/3A.md`](../../camera/3A.md)、[`camera/camera_driver.md`](../../camera/camera_driver.md)、[`camera/android_framework.md`](../../camera/android_framework.md)。

---

## 3. 核心子系统

### 3.1 RAW 域（像素还没变成好看的图）

顺序稳定，白板按这个说：

| 级 | 作用 | 不做的后果 |
|----|------|------------|
| **BLC** | 减暗电流/pedestal | AE/AWB 基准错，阴影发灰 |
| **LSC** | 补偿中心亮四周暗、色阴影 | 四角色偏，夜景更明显 |
| **BPC** | 静动态坏点 | demosaic 后彩点 |
| **Bayer NR** | 马赛克域去噪 | 噪点被插值放大 |
| **Demosaic** | Bayer → RGB | 拉链、假色 |
| **（可选）RAW HDR / 线性化** | 拼接或 tone 前保持线性 | 高光死黑、合成鬼影 |

Stats 抽头必须在 **LSC 之后、重 tonemap 之前** 的线性域，否则 3A 看到的是已经被美化的直方图。

眼镜：LSC/OTP 表随 SKU；温度升则 black level 漂移 → BLC 用 **gain×温度 二维表**，不是一个常数。

### 3.2 3A 闭环

```
IFE Stats (帧 N) → 3A 算法 (DSP)
        → 决策 exposure/gain/CCM/AF
        → CCI 写入 Sensor（生效于帧 N+2 左右）
        → ISP 数字增益 / CCM 用于帧 N+1
```

**闭环带延迟。** 不要说“这一帧 Stats 改这一帧曝光”——积分已经结束。

| 环 | 传感器/ISP 执行器 | 稳定策略 |
|----|-------------------|----------|
| AE | 曝光时间、again、dgain | 目标亮度 + 高光保护；录像锁 fps 时优先改 gain |
| AWB | R/B 增益、CCM 选择 | 场景分类防室内外来回跳 |
| AF | VCM 位置（眼镜可能 PDAF 弱或定焦） | 眼镜常 **定焦 + 超深**，AF 可能不存在，主动说 |

收敛：预览要 **无振荡**（限制每帧曝光变化率）。拍照前可允许短猎焦/AE lock。

HAL3：每个 Request 带 `CONTROL_AE_MODE` 等；Result 带回实际 exposure。ZSL 环形缓冲必须存 **RAW + 元数据**，合成时用该帧自己的 3A，而不是“当前预览的 3A”。

### 3.3 YUV / RGB 域

Demosaic 之后才是观感：

- **CCM / 色校正** → 目标色域（sRGB / Display P3）
- **Tone mapping / Gamma** → 10/12-bit 线性压到 8/10-bit 显示
- **TNR / 空间 NR** → 录像用时域；预览用轻量以免拖影
- **Sharpen** → 在 NR 之后，避免把噪声当边缘
- **Color enhancement** → 产品风格；IQ 表，不是硬编码

视频还要：**EIS**（gyro + RS 行时间）、**音画 PTS**（同一 SoC 时间基）。

### 3.4 驱动与框架解耦

三层契约，面试按层找 bug：

```
Camera2 API          应用要什么 stream（preview / video / yuv / jpeg）
    ↓
HAL3                 异步：configureStreams / processCaptureRequest
                     Buffer + sync fence + metadata 对齐成一帧
    ↓
CamX graph           把 Request 编译成 IFE/IPE/BPS Node
    ↓
V4L2 / KMD           寄存器 + QBUF/DQBUF；不理解 Android 的 Surface
```

**解耦原则：**

- Kernel 不跑 3A、不知 JPEG 质量。
- HAL 不直接 `memcpy` 像素；只传 dma-buf fd。
- App 不配 MIPI lane。Vendor tag 给 IQ / 扩展，不把驱动寄存器暴露给应用。

Media Controller 把 Sensor–CSID–IFE–IPE 连成 graph，usecase 切换 = 重链，不是在用户态 if-else 拷图。

---

## 4. 性能优化与边界

| 目标 | 做法 |
|------|------|
| 预览延迟 | IFE 直出缩小 YUV；跳过 BPS 全尺寸；TNR 半径减小 |
| 4K 带宽 | UBWC / AFBC 压缩 YUV；NV12 不转 RGB；encoder 直接吃 dma-buf |
| 掉帧 | 查哪一层 DQBUF 超时：Sensor SOF、IFE、IPE、encoder、App 持锁。见 [`camera/掉帧.md`](../../camera/掉帧.md) |
| Stripe / tile | 全尺寸 NR 按条带过 IPE，降低 on-chip SRAM |
| 热 | 录像 5 分钟后降 4K→1080p 或 60→30；AE 目标略降（暗一点少 gain 少噪再少 NR） |

恢复：SOF timeout → reset CSI 链路而非重启整个 HAL session；IPE 超时 → 丢该 Request、预览沿用上一帧 fence。

---

## 5. 跨团队落地

| 团队 | 契约 |
|------|------|
| Sensor | Mode 表（HTS/VTS、MIPI DT/VC、黑电平）、OTP |
| IQ Tuning | LSC mesh、CCM、gamma、NR 强度；**按 SKU + 色温 + lux 分 bin** |
| 算法 | 多帧对齐、夜景融合接口：输入线性 RAW + 每帧 metadata |
| Framework | Stream 组合合法性（不能同时 4 路全尺寸 RAW） |
| 实验室 | Golden sample；改 ISP FW 必须重出 IQ 包 |

**收口金句：** ISP 是一条 **带 Stats 抽头的硬件数据流**；软件框架只调度 Request 和 buffer。3A 是跨帧反馈，不是“当前帧滤镜”。
