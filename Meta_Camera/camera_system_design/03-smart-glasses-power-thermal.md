# 场景 3：智能眼镜 — 低功耗、热约束、AI 唤醒与连续录像

**典型题：** 为 Ray-Ban 类智能眼镜设计 AI 视觉唤醒 + 连续录像系统。

**核心考点：** 异构功耗分层、Zero-Copy DMA-BUF / ION、热节流时降帧 / 降分辨率且要平滑。

功耗数字与级联唤醒叙事对齐 [`company/openai/smart-glasses-ai-runtime.md`](../../company/openai/smart-glasses-ai-runtime.md)。

---

## 1. 需求与约束

贴脸设备 **热是产品约束，不是优化项**。

| 维度 | 默认假设 |
|------|----------|
| 整机功率 | **1.5–2.5 W** 峰值；待机 / AON ≪ 100 mW |
| 电池 | 400–600 mAh → 连续录像以小时计要靠降档 |
| 内存 | 2–4GB 共享；CPU/GPU/NPU/ISP 抢同一套 LPDDR |
| 延迟 | 视觉唤醒 **< 100–200 ms** 到主摄出第一帧；预览 <30ms |
| 体验 | 长按录像不烫耳；误唤醒率低；隐私（指示灯 / LED） |
| 传感器 | ULP 视觉或 IMU+麦克风 做 AON；主摄 1080p 级 RS |

**开场金句：** 主 SoC + 全 ISP + 编码器不能 Always-on。架构中心是 **级联唤醒状态机 + 热状态机**，像素全程 dma-buf，CPU 不拷 1080p。

---

## 2. 高层架构

```
[IMU / 触摸 / 语音 DSP]          [ULP 视觉 Sensor 或主摄 subsample]
         │                              │ 极低 MIPI 或 SPI
         ▼                              ▼
    Always-on MCU/DSP              运动 / 简易人物检测（mW 级）
         │                              │
         └──────── 唤醒条件满足 ────────┘
                         ▼
              打开主 MIPI + IFE + 有限 IPE
                         │
                         ▼ dma-buf
              ┌──────────┼──────────┐
              ▼          ▼          ▼
           预览/取景   录像 Encoder  NPU（手势/场景，短 burst）
```

状态（只允许这些跳转，带滞回）：

```
S0 Sleep        AON IMU/语音     ~1–10 mW
S1 ULP vision   低分辨率检测     数十 mW
S2 Peek         主摄短预览/抓拍  数百 mW–1W 数百 ms
S3 Record       1080p30 + encode 接近热墙
S4 Throttle     720p15 / 关 NPU  守皮肤温
```

S3 不能直接回 S0（用户在录）；只在 S4 与 S3 之间按温度滞回。

---

## 3. 核心子系统

### 3.1 异构计算与功耗分层

| 层 | 硬件 | 干什么 | 不干什么 |
|----|------|--------|----------|
| AON | 独立 DSP/MCU | VAD、敲击、IMU wake | 跑 ISP、跑大模型 |
| ULP CV | 小阵列或主摄 binning + 微型 CV | 运动、或许 HOG/tiny CNN | 4K、3A 猎焦 |
| 主 ISP | IFE + 轻 IPE | 录像 / 预览 | 夜景 10 帧融合（改抓拍态） |
| NPU | Burst | 唤醒确认、场景、语音视觉 | 持续 30fps 大模型 |

唤醒策略（降低误触）：

1. IMU 或语音过门槛 → S1  
2. ULP 确认“像使用意图”（举镜、有人脸/手）→ S2 开主摄  
3. NPU 二次确认才进 S3 长录  

主摄启动成本：PLL lock、MIPI settle、第一帧 3A。为压到 200ms：**预保持 Sensor 在 standby 而不是 power-off**（漏电 vs 延迟的权衡，产品定）。

### 3.2 内存与带宽削峰（Zero-Copy）

录像时三条消费者：encoder、（可选）预览、（可选）NPU。错误做法：IFE → CPU memcpy 三份。

正确：

```
Gralloc / dma-buf 分配 NV12（或 UBWC）
IFE/IPE 写 fd
        ├─ Encoder import fd   （HW）
        ├─ GPU/Display import  （预览，可降分辨率另一条 stream）
        └─ NPU import fd       （若要 AI，用缩小的 analysis stream）
sync fence：生产者 signal，消费者 wait
```

ION 是高通历史上的连续内存堆；新平台是 **dma-buf heap**。面试说：“用户态只传 fd 与 fence，SMMU 映射给各 IP。”

削峰：

- Analysis 流 320×240 给 NPU，不要喂 1080p。
- Encoder 与 ISP 协商 UBWC，少 30–50% DDR。
- 禁止预览 RGB 888。
- 环形缓冲深度 3；App 持 buffer 过久 → 掉帧，用超时回收（HAL3 buffer 管理）。

Cache：DMA 写后 CPU 若要读（调试 dump），必须 invalidate；热路径 CPU 不读像素。

### 3.3 热节流平滑策略

皮肤温 / 结温 / 电池温 三个传感器。策略要 **单调、有滞回、用户可感知但不闪烁**。

建议阶梯（示例，数字面试时声明是假设）：

| 档 | 触发（示意） | 动作 |
|----|--------------|------|
| T0 | 正常 | 1080p30 + 轻量 NPU 1Hz |
| T1 | 皮肤温升 | 关 NPU 连续推理；ISP NR 降档 |
| T2 | 再升 | **30 → 24 或 15 fps**（先降帧，分辨率暂时不变） |
| T3 | 接近限 | **1080p → 720p**，encoder GOP 简化 |
| T4 | 硬限 | 停录、提示过热；Sensor 回 S1 |

为什么先降帧再降分辨率：分辨率切换要 **重 configureStreams**（闪一下、要重新 3A）；降 fps 可在 sensor VTS 上改，体验更连续。

防抖振：升档阈值 < 降档阈值（例如 41°C 降、37°C 才升），最短停留 15–30s。

录像文件：fps 变化要写进容器时间戳，播放器按 PTS，不要假装一直 30fps。

---

## 4. 性能优化与边界

- **Always-on 误唤醒：** 用双因子（运动+视觉）；记录隐私灯与本地日志（无图）。
- **第一帧发黑：** Sensor 从 standby 起来 AE 未收敛 → 用 ULP 估的 lux 做 **warm start exposure**。
- **边录边传：** Wi-Fi 上传不得阻塞 ISP 线程；独立低优先级进程，热档 T2 以上禁止后台编码第二路。
- **掉帧：** 见 [`camera/掉帧.md`](../../camera/掉帧.md)。眼镜上优先假设 **热和 DDR**，再查 MIPI。
- **隐私：** 物理指示灯与 ISP streamon 绑定，软件关不掉（HW 或安全元件）。

---

## 5. 跨团队落地

| 团队 | 契约 |
|------|------|
| 工业设计 / 热 | 皮肤温测点位置、限值、风扇（通常没有）→ 软件阶梯必须匹配实测 |
| Sensor | Standby 电流 vs 出图时间；ULP 模式寄存器 |
| 系统 PM | DVFS 表：T2 时砍哪些 CPU/NPU 频点，**留给 IFE+VPU** |
| 产品 | 降帧是否在 UI 显示；过热停录文案 |
| 隐私 / 法律 | LED、录音灯、数据是否出设备 |
| IQ | 15fps / 720p 各出一套 NR，避免降档后突然“更糊或更噪” |

**收口金句：** 眼镜 Camera 系统是一台 **热管理状态机**，ISP 只是 S2/S3 的执行器。零拷贝决定你能不能在 2W 里录像；级联唤醒决定电池能不能撑过一天。
