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

---
---

# E6 深度层

---

## 6. 数字预算

### 6.1 带宽：4K30 录像会把 DDR 打爆，这是这题的核心矛盾

4K30 NV12 单次通过：

$$
3840 \times 2160 \times 1.5 \times 30 = 373\ \mathrm{MB/s}
$$

**但一帧要过 DDR 好几次。** 老实数一遍：

| 通路 | 读 | 写 | 小计 |
|------|----|----|------|
| IFE 写线性 RAW（若走 offline BPS） | — | 12MP RAW10 ≈ 373 MB/s | 373 |
| IPE 读 RAW / 中间 YUV | 373 | — | 373 |
| **TNR 读上一帧参考** | 373 | — | 373 |
| IPE 写 NV12 输出 | — | 373 | 373 |
| Encoder 读 | 373 | — | 373 |
| 预览分支（1080p，缩放后） | 93 | 93 | 186 |
| **合计** | | | **≈ 2.05 GB/s** |

对照可用带宽 10–25 GB/s，看起来还好——**但相机不是唯一用户**：显示合成、GPU、系统全都在抢，而且 DDR 的**有效带宽**在多客户端随机访问下远低于理论峰值（通常只能拿到 60–70%）。2 GB/s 的相机流量在手机上是"大客户"，在眼镜级 SoC（3–8 GB/s 总带宽）上**直接不可行**。

**三个可用的削减手段，按性价比排序：**

| 手段 | 省多少 | 代价 |
|------|--------|------|
| **UBWC / AFBC 压缩** | 30–50%，约 700 MB/s | 需要 IPE 和 encoder 都支持同一压缩格式；调试时不能直接 dump |
| **Streaming / inline 模式**（IFE 直连 IPE，不落 DDR） | 省掉 RAW 的写+读，约 750 MB/s | 需要 SoC 支持；限制了可插入的处理和 tiling 自由度 |
| **降 TNR 的参考帧位深或分辨率** | 约 190 MB/s | 降噪质量下降 |

按 100 mW/(GB/s) 换算，**UBWC 省下的 700 MB/s ≈ 70 mW**——这在眼镜上是能不能录像的差别。

> **E6 表述：** "我先算出 2 GB/s，然后说这在手机上勉强、在眼镜上不可行，所以眼镜的媒体路径必须是 1080p 而不是 4K，并且 UBWC 是**必需项不是优化项**。这是我会在架构评审上守住的一条线。"

### 6.2 内存：ZSL 环是最大的单笔支出

12MP RAW10 packed 单帧 $= 12\mathrm{M} \times 1.25 = 15$ MB。

| ZSL 环深度 | 内存 | 能力 |
|-----------|------|------|
| 4 | 60 MB | 零快门延迟 + 最多 4 帧多帧融合 |
| 8 | 120 MB | 舒服的夜景融合窗口 |
| 12 | 180 MB | 大部分场景够，但眼镜上不可能 |

在 2–4 GB 共享内存的眼镜上，**120 MB 的 ZSL 环是一个要拿到产品评审桌上讨论的数字**，不是工程师自己决定的。

> **决策表述：** "眼镜上我会把 ZSL 环砍到 3–4 帧，接受多帧夜景只能融 3 帧；或者干脆不做 ZSL，接受 100–200 ms 快门延迟换 60 MB。哪个对——取决于产品认为'按下就拍到'和'夜景画质'哪个更重要。**这是我需要产品经理回答的问题，不是我单方面能定的。**"

### 6.3 延迟：预览的 glass-to-glass 账本

1080p30，RS 传感器：

```
曝光                       8 ms（低光；亮光下 2 ms）
readout SOF→EOF           ≈ 30 ms 的 0.7–0.9 → 约 25 ms   ← 支配项
IFE 流水线穿透              < 1 ms（行级流水，不等整帧）
IPE 处理                    3–5 ms
合成器等一个 vsync          16.7 ms @60Hz（最坏）
显示扫描出                  ~8 ms
────────────────────────────────────────
合计                       ≈ 60–65 ms
```

**这已经超过"< 30 ms"的目标了。** 老实说出来，然后给出真正能动的三个杠杆：

1. **提帧率**：60fps 采集把 readout 从 25 ms 砍到 12 ms。这是**最大的一刀**，代价是功耗和带宽翻倍。
2. **IFE 直出预览，绕过 IPE**：省 3–5 ms，代价是预览画质和最终成片不一致（用户会注意到）。
3. **合成器 late latching / 直通层**：把预览 buffer 直接给显示控制器的一个 overlay plane，绕过 GPU 合成，省掉一个 vsync。

> **关键洞察（要说出来）：** "**readout 时间是支配项，而它由帧率决定。** 所以'优化预览延迟'的第一个动作永远是提采集帧率，而不是优化 ISP 代码。很多人会花两周去优化那 3 ms 的 IPE，那是走错方向。"

### 6.4 3A 收敛：把 N+2 变成时间

曝光生效是 **N+2 帧**。30fps 下就是 **67 ms 的控制环延迟**。

AE 收敛需要几步？带阻尼的一阶环，每帧向目标走 30%（防震荡），从 4 倍误差收敛到 10% 以内约需 **9–10 帧 ≈ 300 ms**。

$$
\text{收敛时间} \approx \frac{\ln(\epsilon / e_0)}{\ln(1-k)} \times \frac{1}{\text{fps}},\quad k = \text{每帧步进比例}
$$

**这解释了两个产品现象：**
- 从口袋掏出相机的第一秒画面会明显变亮变暗——这是 AE 在收敛，不是 bug
- 想让它更快就得提 $k$，但 $k$ 太大会震荡（因为环里有 67 ms 的死时间）

**缓解手段：** 冷启动时用**上次的 AE 结果做种子**（持久化到 NVM），或者用 ULP sensor 估的 lux 做 warm start。眼镜上这一条尤其重要，因为"抬手就拍"的窗口只有几百毫秒。

---

## 7. 关键决策与被否方案

| 决策 | 我选 | 否掉的 | 为什么 | 什么会让我翻盘 |
|------|------|--------|--------|----------------|
| Stats 抽头位置 | **LSC 之后、tonemap 之前的线性域** | BE 的 YUV 域 | 3A 必须看线性光；在被 tone curve 美化过的数据上做 AE 会形成正反馈（越亮越压、越压越提） | 无。这条没有例外 |
| 预览来源 | **IPE 处理后**（与成片同一套 IQ） | IFE 直出缩小 YUV | 预览和成片色彩/锐度不一致是可见的产品缺陷 | 延迟成为首要指标（passthrough 场景），此时接受不一致换 3–5 ms |
| ZSL 存什么 | **线性 RAW + 该帧自己的完整 metadata** | 存 YUV | 存 YUV 就锁死了后处理空间，多帧融合和 HDR 都做不了 | 内存实在放不下（超低端 SKU），退化成存 YUV + 不做多帧 |
| 多帧融合的 3A | **用每帧自己的 3A 参数** | 用快门瞬间的当前 3A | ZSL 环里的帧是不同时刻拍的，曝光可能已经变了；用当前参数会导致亮度跳变和鬼影 | 无 |
| AE 执行器优先级 | **录像锁 fps 时优先改 gain；拍照优先改 exposure** | 统一策略 | 录像改 exposure 会改帧率（VTS 联动）导致音画不同步；拍照提 gain 会增噪 | 极低光录像，此时接受降帧率换噪声 |
| Flicker 检测 | **专用行均值统计 + 频域判 50/60Hz** | 用户手动设置地区 | 跨境用户、混合光源（LED 驱动频率各异）下手动设置必错 | 传感器不提供行统计，只能退回手动 + 默认按地区 |
| 掉帧恢复粒度 | **复位 CSI 链路，保留 HAL session** | 重启整个 camera session | 重启 session 用户会看到黑屏 1–2 秒；复位链路 100 ms 内可恢复 | 复位链路无法清除错误状态（某些 SoC 的已知问题），只能升级为 session 重启 |

**关于 demosaic 之后的顺序，有一条值得单独讲：**

> **NR 必须在 sharpen 之前。** 反过来会把噪声当成边缘锐化出来，产生"油画感 + 噪点被强化"的双重灾难。这条顺序是硬约束，不是调优选择。同理 **CCM 在 gamma 之前**（CCM 是线性域的矩阵运算，放到 gamma 之后色彩会错）。

---

## 8. 失效模式与降级

| 失效 | 检测 | 恢复 | 用户可见 |
|------|------|------|----------|
| **SOF timeout** | 硬件定时器；连续 N 帧无 SOF | 复位 CSIPHY/CSID 链路，重下 sensor mode；保留 HAL session | 短暂卡顿 |
| **IPE 超时 / hang** | watchdog（例如 2 帧周期） | 丢弃该 Request，向上报 error result；预览沿用上一帧 | 掉 1 帧 |
| **Buffer 饥饿**（App 持有太久不还） | 池的 in-use 高水位 + HAL buffer 超时 | 强制回收超时 buffer；持续则降低该 stream 的队列深度 | 帧率下降 |
| **3A 震荡（hunting）** | 曝光值的帧间变化率超过阈值持续 N 帧 | 临时降低环增益 / 强制进入锁定态；上报遥测 | 画面忽明忽暗 |
| **AWB 在单色场景失效**（对着一面红墙） | 色度直方图的聚集度 + 灰点数量不足 | 保持上一次可信的白平衡（AWB lock），不要盲目跟随 | 无（正确行为）；错误行为是画面整体偏色 |
| **LSC 表与模组不匹配**（换模组没更新标定） | 四角亮度/色度残差超限（可在出厂自检时测） | 拒绝加载不匹配的标定；用通用回退表并告警 | 四角偏色 |
| **黑电平随温度漂移** | 遮光像素（OB）区域的实测值 vs 表值 | 用 OB 区实时校正而非纯查表 | 暗部发灰或发黑 |
| **录像热降级** | thermal zone | 4K→1080p 或 60→30fps；**必须写进容器 PTS** | 分辨率/流畅度下降 |
| **编码器背压** | encoder 输入队列积压 | 丢非关键帧或降码率；**不要阻塞 ISP 线程** | 码率波动 |

**一条设计原则：** 编码器、上传、AI 分析都是**下游可失败的消费者**，它们的任何问题都不允许反压到 ISP 数据面。实现上就是：这些消费者用独立的 buffer 池 + 有界队列 + 丢弃策略，永远不让 ISP 的 QBUF 等它们。

---

## 9. 怎么证明它是对的

### 9.1 IQ 实验室（客观指标）

| 指标 | 图卡 / 方法 | 说明 |
|------|------------|------|
| **SFR / MTF50** | 斜边卡（ISO 12233） | 分辨率与锐化的量化，能抓住"过锐化"（MTF 曲线出现 > 1 的过冲） |
| **色彩 ΔE** | ColorChecker 24 色卡 | 在多个色温光箱下测（2856K / D65 / TL84 / Horizon）；ΔE00 < 5 是常见目标 |
| **SNR / 动态范围** | 灰阶楔（OECF） | 按 lux 扫（1 / 10 / 100 / 1000 lux） |
| **纹理保留** | Dead Leaves 卡 | 抓"NR 把细节磨没了"——这是纯 SNR 指标看不出来的 |
| **暗角 / 色阴影** | 均匀积分球 | 验证 LSC |
| **闪烁** | 可调频灯箱（50/60Hz + LED PWM） | 验证 flicker 检测 |
| **几何畸变** | 棋盘格 | 验证内参和畸变校正 |

**E6 的补充：** 客观指标只能防退步，**不能定义好看**。所以必须配一套**主观 A/B 流程**（盲测、固定显示设备、多人打分），以及一个**失败案例语料库**（历史上出过问题的场景：逆光人脸、烛光、雪地、霓虹招牌、绿植）。每次 IQ 改动都要过这个语料库。

### 9.2 RAW 回放回归（regression 的基石）

```
语料：几百到几千段 RAW + 完整 metadata（曝光、gain、色温、传感器型号）
回放：灌进 ISP（HW 回放模式或 bit-exact 的 C 模型）
比对：与基线输出逐像素 diff / 客观指标 diff
```

**关键要求：ISP 要能 offline 回放 RAW**，这在硬件上通常是支持的（BPS 的 offline 模式）。有了它：
- IQ tuning 改一个参数，10 分钟跑完全部语料，而不是重新拍
- 每次 ISP FW 升级都能做 bit-exact 比对，任何非预期的像素变化立刻暴露
- 出现现场问题时，用户上传的 RAW 能在本地精确复现

**没有 RAW 回放的相机团队会陷入"每次改动都靠人肉重拍"的泥潭**，这是 E6 会主动建立的基础设施。

### 9.3 时序与稳定性测试

- **长跑（soak）**：连续录像 2 小时，监控掉帧、内存、温度、文件完整性
- **压力组合**：4 路 stream 同开 + 频繁 configureStreams 切换 + 反复 open/close
- **故障注入**：人为触发 SOF timeout、IPE hang、buffer 超时，验证 §8 的恢复路径
- **冷启动计时**：从 `open()` 到第一帧可用的 p50/p95（这是用户能感知的核心指标）

### 9.4 Fleet SLI

| SLI | 阈值示例 |
|-----|---------|
| 冷启动到首帧 p95 | < 500 ms |
| 掉帧率（分层：sensor/CSI/IFE/IPE/App） | 各层 < 0.1% |
| AE 收敛时间 p95 | < 500 ms |
| AWB 置信度低的会话占比 | < 5% |
| 拍照失败率（超时 / 错误 result） | < 0.01% |
| 录像文件损坏率 | ~0 |
| Camera session 异常终止率 | < 0.1% |

**OTA 之后按 SKU + 传感器批次分组对比**，任一组回归即 halt。

---

## 10. 演进与组织

### 10.1 契约

| 契约物 | Owner | 消费者 | 绑定关系 |
|--------|-------|--------|----------|
| Sensor mode 表（HTS/VTS、DT/VC、黑电平、OTP 布局） | Sensor 厂 + FW | 驱动、3A | 与驱动版本绑定 |
| **IQ 调优包**（LSC mesh、CCM、gamma、NR 强度，按 SKU × 色温 × lux 分 bin） | IQ Tuning | ISP FW | **与 ISP FW 版本强绑定**，OTA 必须同包 |
| Vendor tag 定义 | 你 | App / 上层 | 只增不改；废弃要走 deprecation 周期 |
| Stream 组合合法性矩阵 | 你 | Framework / App | 明确写出"不支持 4 路全尺寸 RAW 同开"这类限制 |
| 帧 metadata schema | 你 | 算法、App | 语义变更必须 bump 版本 |
| Golden sample 设备 | 实验室 | 所有人 | 每个 SKU 至少 3 台封存 |

**最容易出事的是第 2 行。** "新 ISP FW + 旧 IQ 包"会产生非常隐蔽的画质问题（不崩溃、只是色彩略偏），而且往往在灰度发布几天后才被用户投诉发现。**解法是把兼容性检查做成硬失败**：启动时校验 IQ 包的版本戳与 FW 匹配，不匹配就拒绝加载并用安全回退表 + 上报遥测。

### 10.2 分阶段

| 阶段 | 出口标准 |
|------|----------|
| P0 出图 | Sensor 出 RAW，能 dump，黑电平和 Bayer pattern 确认正确 |
| P1 预览通路 | 3A 闭环稳定不震荡；预览延迟实测 |
| P2 录像与拍照 | ZSL 打通；编码音画同步；长跑 2 小时无泄漏 |
| P3 IQ | 客观指标达标；主观 A/B 通过；RAW 回放语料建立 |
| P4 量产 | 产线标定 + 自检；fleet SLI 上线；IQ 包版本绑定机制生效 |

**RAW 回放语料要在 P1 就开始积累**（哪怕 IQ 还很差），因为它的价值来自覆盖面，而覆盖面需要时间。

**收口金句（E6 版）：** ISP 是一条**带 Stats 抽头的硬件数据流**，软件框架只调度 Request 和 buffer；3A 是跨帧反馈，不是当前帧滤镜。而让这条流水线可交付的，是两件基础设施：**RAW 离线回放**（让 IQ 改动可回归）和 **IQ 包与 FW 的强版本绑定**（让灰度发布不会产生无人能复现的画质投诉）。
