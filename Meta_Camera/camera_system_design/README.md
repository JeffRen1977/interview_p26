# Camera System Design（AR / VR / 可穿戴）

Meta 电面里 Camera **领域系统设计**看的是：端到端 Pipeline、功耗 / 热、延迟、以及你能不能和 Sensor / 算法 / IQ 对着同一套约束把系统落地。

本目录按官方建议的 **4 个核心场景** 演练。总备考清单：[`../camera_software_engineer_prep.md`](../camera_software_engineer_prep.md)。底层细节仍以仓库笔记为准，这里写的是 **面试口述架构**。

| 场景 | 文件 |
|------|------|
| 答题框架（5 步） | 本页 |
| 6DoF SLAM / Controller 多相机同步 | [01-multi-camera-slam-sync.md](./01-multi-camera-slam-sync.md) |
| 端到端拍照 / 视频 ISP | [02-e2e-isp-pipeline.md](./02-e2e-isp-pipeline.md) |
| 智能眼镜低功耗与热约束 | [03-smart-glasses-power-thermal.md](./03-smart-glasses-power-thermal.md) |
| AI + 传统 ISP 混合 | [04-ai-isp-hybrid.md](./04-ai-isp-hybrid.md) |

相关笔记：[`camera/sensor.md`](../../camera/sensor.md) · [`camera/3A.md`](../../camera/3A.md) · [`camera/camera_driver.md`](../../camera/camera_driver.md) · [`camera/android_framework.md`](../../camera/android_framework.md) · [`camera/掉帧.md`](../../camera/掉帧.md) · [`company/openai/smart-glasses-ai-runtime.md`](../../company/openai/smart-glasses-ai-runtime.md)

---

## 系统设计答题框架（5 步）

白板时间大约 **25–35 分钟**。不要一上来画 ISP 每一级；先把约束钉死。

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

## 面试时怎么用这四篇

每篇结构都按上面 5 步写。闭卷练习：抽一篇，12 分钟画完图并讲完一个深挖点。细节公式可以指回 `camera/3A.md` / `sensor.md`，白板不要背模块寄存器名。
