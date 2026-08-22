# Camera System Design（AR / VR / 可穿戴）— E6 版

Meta 电面里 Camera **领域系统设计**看的是：端到端 Pipeline、功耗 / 热、延迟、以及你能不能和 Sensor / 算法 / IQ 对着同一套约束把系统落地。

本目录按官方建议的 4 个核心场景 + 5 个补充场景演练。总备考清单：[`../camera_software_engineer_prep.md`](../camera_software_engineer_prep.md)。底层细节仍以仓库笔记为准，这里写的是 **面试口述架构**。

> **GitHub 阅读：** 公式只用 `$...$` / `$$...$$`，数学块内不放中文、`°`、全角括号或 en-dash（`–`），否则网页端 MathJax 会渲染失败。

| # | 场景 | 文件 |
|---|------|------|
| — | 答题框架 + E5/E6 分界 | 本页 |
| 01 | 6DoF SLAM / Controller 多相机同步 | [01-multi-camera-slam-sync.md](./01-multi-camera-slam-sync.md) |
| 02 | 端到端拍照 / 视频 ISP | [02-e2e-isp-pipeline.md](./02-e2e-isp-pipeline.md) |
| 03 | 智能眼镜低功耗与热约束 | [03-smart-glasses-power-thermal.md](./03-smart-glasses-power-thermal.md) |
| 04 | AI + 传统 ISP 混合 | [04-ai-isp-hybrid.md](./04-ai-isp-hybrid.md) |
| 05 | 眼动 / 面部追踪与 Foveated Rendering | [05-eye-face-tracking.md](./05-eye-face-tracking.md) |
| 06 | Passthrough 重投影与 EIS | [06-passthrough-reprojection.md](./06-passthrough-reprojection.md) |
| 07 | 多客户端相机仲裁与隐私 | [07-camera-arbitration-privacy.md](./07-camera-arbitration-privacy.md) |
| 08 | 标定系统（产线 → 在线 → OTA） | [08-calibration-system.md](./08-calibration-system.md) |
| 09 | 相机验证与测试基础设施 | [09-validation-infrastructure.md](./09-validation-infrastructure.md) |

每篇的 §1–§5 是架构主线，**§6–§10 是 E6 深度层**（数字预算 / 决策与否定方案 / 失效模式 / 验证 / 演进）。

相关笔记：[`camera/sensor.md`](../../camera/sensor.md) · [`camera/3A.md`](../../camera/3A.md) · [`camera/camera_driver.md`](../../camera/camera_driver.md) · [`camera/android_framework.md`](../../camera/android_framework.md) · [`camera/掉帧.md`](../../camera/掉帧.md) · [`company/openai/smart-glasses-ai-runtime.md`](../../company/openai/smart-glasses-ai-runtime.md)

---

## E5 和 E6 的分界线

同一道题，两个级别的差别不在于你知道多少模块名，而在于**你的答案能不能被检验**：

| 维度 | E5（能过） | **E6（要拿到）** |
|------|-----------|-----------------|
| 数字 | 说得出量级（"1.5–2.5W"、"<15ms"） | **当场算出来并且收口**：带宽 GB/s 加总、延迟逐项累加、功耗按 block 分配，然后说"所以这个方案在预算内 / 超了，我砍这里" |
| 决策 | 列出 A 和 B 两种做法及优缺点 | **选一个并说为什么**，同时说出 **"什么证据会让我改主意"**，以及被否掉的方案在什么条件下会翻盘 |
| 失效 | 提到"要处理掉帧" | **降级阶梯**：每一档的触发信号、检测手段、恢复动作、用户可见性，外加防抖振的滞回 |
| 验证 | "会做测试" | **具体的台架**：什么 rig、什么金标数据集、什么回放框架、上线后看哪几个 fleet SLI |
| 组织 | "会和 HW 团队沟通" | **契约是产物**：谁 own 什么文件、版本怎么绑定、partner 团队晚交付时你的 plan B、分几个季度落地 |
| 范围 | 回答被问的问题 | **主动收窄或扩大问题边界**，说清哪些是这题的核心、哪些是我明确不做的，并说明为什么这个切分是对的 |

**最强的 E6 信号是第 4 行。** 绝大多数候选人能画对 ISP 流程图，极少数人能说清"我怎么证明它是对的、怎么在 10 万台设备上发现它错了"。每篇的 §9 就是为这一点写的。

---

## 系统设计答题框架

白板时间大约 **25–35 分钟**（onsite 可能 45）。不要一上来画 ISP 每一级；先把约束钉死。

### 45 分钟的时间分配（E6 建议）

| 时间 | 做什么 | 不要做什么 |
|------|--------|-----------|
| 0–5 | Clarify + **主动报一张假设表** + 说出"这题我要证明的三件事" | 反复问细节问到面试官不耐烦 |
| 5–12 | 高层架构，**数据面 / 控制面分开画** | 画 ISP 每一级 |
| 12–20 | **当场算预算**：带宽、延迟、功耗、内存 | 只报量级不算数 |
| 20–32 | 深挖 1–2 个子系统（面试官挑，或你推荐） | 平均用力挖五个 |
| 32–38 | 失效模式 + 降级阶梯 | 只说"会做错误处理" |
| 38–43 | 验证方案 + 演进路线 | 跳过 |
| 43–45 | 反问 | 不问 |

**开场 60 秒的模板：**

> "我先把这题切成三块：①（数据通路）②（控制与闭环）③（约束：功耗/延迟/热）。我会重点挖 ② 和 ③，因为这里的失败是系统性的，①更多是工程量。我先报一组假设，你觉得不对随时打断——[假设表]。这题我要证明三件事：[a] 端到端延迟能收进 X ms；[b] 峰值带宽不超过 Y GB/s；[c] 任何一个部件挂掉系统都有可用的降级态。"

这段话本身就是 E6 信号：**你在管理这场对话的范围，而不是被问题牵着走。**

---

## 五步主线

```
[1. 需求与约束 Clarification]
   ↓ 相机路数、分辨率、帧率、端到端延迟、功耗/热、失败模式
[2. 高层架构 High-Level Architecture]
   ↓ Sensor → MIPI CSI-2 → Driver/HAL → HW ISP → DMA-BUF → App/AI/Display
[3. 核心子系统 Deep Dive]
   ↓ 3A 闭环、零拷贝内存、帧同步与时间戳（挑 1–2 个深挖）
[4. 性能优化与边界 Optimization]
   ↓ Tiling、带宽、掉帧恢复、热节流策略
[5. 跨团队落地 Cross-functional]
   ↓ Sensor HW / 算法 / IQ Tuning 的标定接口与版本契约
```

### 1. Clarification（3–5 min）

主动报一组 **可被挑战的默认假设**，而不是问到面试官不耐烦：

| 维度 | 眼镜 / 可穿戴默认 | VR / 头显默认 |
|------|-------------------|---------------|
| 路数 | 1 主摄 + 可选 ULP 视觉 | 4–8 tracking GS + IMU + 可选 passthrough |
| 分辨率 / 帧率 | 主摄 12MP 抓拍 / 1080p30 录像；ULP QVGA 10–30fps | Tracking 640×480 或 800×600 @ 60–90fps |
| 延迟 | 唤醒 <100ms；预览玻璃到玻璃 <30ms | Pose 更新 <5–10ms；MTP <15–20ms |
| 功耗 / 热 | 整机 1.5–2.5W；贴脸皮肤温 | 头显更宽，但仍要看 SoC 结温与风扇策略 |
| 内存 | 2–4GB 共享 LPDDR；禁止热路径 memcpy | 更大，但仍禁止 4K NV12 CPU copy |

再问清：**这题偏 tracking、偏 IQ，还是偏功耗？** 后面只深挖那一条。

### 2. High-Level Architecture（5–8 min）

先画这一条，再往上加支路：

```
Sensor (Bayer / GS tracking)
        │ MIPI CSI-2 (D-PHY/C-PHY)
        ▼
CSIPHY → CSID → IFE/ISP FE
        │
        ├─ Stats ──► 3A (DSP/ARM) ──► CCI 回写 Exposure/Gain
        │
        ▼ dma-buf + fence（零拷贝）
IFE/BPS/IPE 或 CV FE-only
        │
   ┌────┴─────────┬──────────────┐
   ▼              ▼              ▼
Display/GPU     Encoder        NPU / SLAM
```

口述一句：**控制面**（CCI、3A、stream on/off）和 **数据面**（MIPI → DMA）分开；数据面不经过 CPU。

### 3. Deep Dive（10–12 min）

只选与题目最相关的 1–2 个：

- Tracking 题 → FSYNC + IMU–Camera 时间戳 + GS vs RS
- ISP 题 → Stats 从哪抽、3A 控制的是 N+2 帧、HAL3 Request/Result
- 眼镜题 → 级联唤醒 + 热状态机
- AI-ISP 题 → 预览 HW 快路径 vs 抓拍 NPU 慢路径 + QoS

### 4. Optimization（3–5 min）

数字优先：MIPI Gbps、DDR 带宽、掉帧从哪一层补。给出 **降级阶梯**（降 fps → 降分辨率 → 关 AI 节点），避免抖振（hysteresis）。

### 5. Cross-functional（2–3 min）

标定不是“算法的事”：intrinsics / extrinsics / LSC / BLC / 黑电平温度表都有 **owner、文件格式、和 ISP binary 的版本绑定**。OTA 不能把 IQ 包和 FW 拆开乱配。

---

## 白板速算表（E6 必须能当场算）

不要背结论，背**算法**。面试官改一个参数你就要能重算。

### 带宽


$$
\text{MB/s} = W \times H \times \text{bytes/px} \times \text{fps}
$$

| 格式 | bytes/px |
|------|----------|
| RAW8 | 1.0 |
| RAW10 packed | 1.25 |
| RAW10 unpacked (u16) | 2.0 |
| NV12 / YUV420 | 1.5 |
| RGB888 | 3.0 |
| UBWC/AFBC 压缩 NV12 | ~0.8–1.05（内容相关，按 0.7× 省估） |

**每一次经过 DDR 都要单独计一遍。** ISP 写一次 + encoder 读一次 = 2×。TNR 还要读上一帧 = 3×。这是最常被漏掉的一步。

常用锚点（记住两个，其余现算）：
- **1080p30 NV12 单次通过 = 93 MB/s**（1920×1080×1.5×30）
- **4K30 NV12 单次通过 = 373 MB/s**
- VGA GS RAW8 @90fps = 27.6 MB/s（单相机）；4 路 tracking 写+读 ≈ 220 MB/s
- 12MP RAW10 packed 单帧 = 15 MB

参考天花板：手机/头显 LPDDR5 实际可用 **10–25 GB/s**；眼镜级 SoC 可能只有 **3–8 GB/s**，而且相机只能拿其中一部分。

### MIPI CSI-2 链路


$$
\text{payload Gbps} = W \times H \times \text{bits/px} \times \text{fps} \times 10^{-9},\quad
\text{lane rate} \approx \frac{\text{payload} \times 1.15}{\text{lanes}}
$$

1.15 是包头 / blanking / ECC 的粗略开销。D-PHY 常见每 lane 1.5–2.5 Gbps，C-PHY 按 trio 算（约 2.28 bit/symbol）。

**结论通常是：tracking 相机的瓶颈不是 Gbps，是 CSI 口数和 IFE 客户端数。** 主摄 4K 才会顶到 lane rate。

### 功耗（量级锚点，声明是假设）

| Block | 量级 |
|-------|------|
| VGA GS tracking sensor | 30–80 mW |
| 12MP RS 主摄 streaming | 150–300 mW |
| CSIPHY + CSID（每路） | 10–30 mW |
| IFE FE-only | 50–150 mW |
| IPE 全流程 1080p | 100–300 mW |
| HW 编码器 1080p30 | 50–150 mW |
| NPU burst | 0.5–2 W |
| DDR 流量 | **~50–150 mW / (GB/s)** ← 这条最有用 |

最后一条让你能把带宽直接换算成功耗："我省下 400 MB/s 的 UBWC，大约就是 40 mW，占眼镜预算的 2%。"

### 延迟账本（逐项加，不要报总数）

```
曝光中点 → SOF 参考点        （曝光时间 / 2）
SOF → EOF                    readout time ≈ 1/fps 到 0.7/fps
EOF → DMA 写完 + fence       0.1–0.5 ms
ISP 处理（流水线穿透）        0.5–5 ms（看是否 FE-only）
算法 / 合成                   2–10 ms
显示：合成 1 vsync + 扫描     16.7 ms @60Hz / 11.1 ms @90Hz
```

**关键洞察：readout 时间通常是最大的一项，而它由帧率决定。** 想砍延迟先提帧率或用更快的 readout mode，而不是优化软件。

### 热


$$
\Delta T = P \times R_{th},\qquad \tau = R_{th} \times C_{th}
$$

眼镜镜腿量级：$R_{th} \approx 15$–$25\ ^{\circ}\mathrm{C/W}$，$\tau \approx 3$–$10$ 分钟。皮肤长时接触限值约 **43 °C**（塑料可略高）。25 °C 环境 → $\Delta T$ 预算 18 °C → **稳态功率上限 ≈ 0.7–1.2 W**。

$\tau$ 是分钟级，这就是为什么**允许短时 burst 超预算**，也是为什么滞回的最短停留时间要设成几十秒而不是几秒。

### 能量


$$
\mathrm{Wh} = \frac{\mathrm{mAh} \times V}{1000},\qquad t_{\mathrm{h}} = \frac{\mathrm{Wh}}{P_{\mathrm{avg}}}
$$

续航小时 = 瓦时 / 平均功率。

500 mAh @ 3.7V = **1.85 Wh**。2W 连续录像 → **55 分钟**；20 mW 待机 → 92 小时。真实答案要用**混合使用模型**（每天 N 张照片 + M 分钟录像 + 全天待机）而不是单一场景。

---

## 面试时怎么用这九篇

每篇结构都按上面五步 + E6 深度层写。

**闭卷练习顺序：**

1. 抽一篇，**12 分钟**画完架构图并讲完一个深挖点（这是 E5 门槛）
2. 同一篇再来一遍，这次**必须当场算三个数字**并让它们收口（E6 门槛）
3. 让别人随机问一个失效模式，你要能立刻给出检测信号 + 降级动作
4. 用一句话说清"我怎么在 10 万台设备上发现这个子系统坏了"

细节公式可以指回 `camera/3A.md` / `sensor.md`，白板不要背模块寄存器名。**但速算表里的算法要背下来。**
