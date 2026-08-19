# 场景 4：AI + 传统 ISP 混合 — RAW 域深度学习进 Pipeline

**典型题：** 如何把 Raw-domain 深度学习去噪 / 夜景超分集成进现有 Camera Pipeline？

**核心考点：** 前/后处理分流、实时预览 HW 快路径 vs 抓拍 NPU 慢路径并发、内存与算力 QoS 隔离。

传统 ISP 级序见 [`02-e2e-isp-pipeline.md`](./02-e2e-isp-pipeline.md) 与 [`camera/3A.md`](../../camera/3A.md)。

---

## 1. 需求与约束

| 维度 | 假设 |
|------|------|
| 预览 | 必须 **HW ISP 实时**，<30ms，不能等 NPU 夜景模型 |
| 抓拍 | 夜景 / 超分可 **100–800 ms** 甚至数秒（多帧），要有 UI 进度 |
| 模型 | RAW denoise 吃 **线性 Bayer**（BLC/LSC 后）；超分常在 RGB/YUV |
| 算力 | NPU 与 IFE/IPE/Encoder **抢 DDR 和电源轨** |
| 兼容 | 关掉 NPU 或模型加载失败时，HW ISP 仍能出 JPEG |

**开场金句：** AI 不是替换 ISP，是 **插在线性 RAW 上的可选 Node**。预览永远不走这条 Node；抓拍才分流。用 QoS 保证 tracking/预览不被夜景模型打死。

---

## 2. 高层架构

```
Sensor Bayer
    ▼
IFE：BLC → LSC → BPC → Stats（3A 仍在这里，实时）
    │
    ├──────────────────────────────┐
    ▼                              ▼
HW 快路径（每帧）              抓拍慢路径（用户快门）
IFE/IPE：demosaic 轻 NR        把「线性 RAW 环」里 N 帧
tone + scale → Preview         dma-buf 交给 NPU RAW denoise
Encoder ← 录像走快路径          → 融合 / 超分（GPU/NPU）
                               → CCM/Gamma 可用 HW IPE 收尾
                               → JPEG
```

关键：**3A 不依赖 NPU 输出。** 否则预览 AE 会等夜景网络，系统锁死。

---

## 3. 核心子系统

### 3.1 前处理 / 后处理分流

模型训练域必须和运行域一致。推荐切分：

| 阶段 | 放 HW ISP | 放 NPU | 原因 |
|------|-----------|--------|------|
| BLC / LSC / BPC | ✓ | | 标定表、线性、便宜 |
| Stats / 3A | ✓ | | 实时闭环 |
| RAW denoise / 拆马赛克联合网络 | | ✓ | 非线性、内容自适应 |
| Demosaic | 快路径 HW；慢路径可融合进网络 | 二选一，**不要 HW demosaic 完再喂 RAW 网络** |
| CCM / Gamma / Sharpen | 快路径 HW；慢路径可 HW 收尾 | 保持品牌色 |

错误切分：

- 先 tonemap 成 8-bit 再“AI 去噪”→ 高光信息没了。  
- 预览也进 NPU RAW 网 → 延迟和功耗双死。  
- 超分放在 Bayer 上但训练用 sRGB → 色偏。

夜景超分更常见：**N 帧 RAW align（gyro + 光流）→ 融合网络 → 一次 demosaic/tonemap**。对齐在 DSP/GPU，卷积在 NPU。

### 3.2 双路径并发调度

HAL3 上这是两条 **stream / usecase**，不是 if 像素：

```
Session 配置：
  Stream A: 1080p YUV preview     → IPE 快路径
  Stream B: 1080p NV12 video      → IPE + VPU
  Stream C: FULL RAW private      → 环形 ZSL（仅 metadata + RAW fd）
快门：
  从 RAW 环取 M 帧 → 提交 CaptureRequest(still)
  快路径继续跑，预览不准停
  AI Node 异步；完成后再 processCaptureResult(JPEG)
```

调度器规则：

1. **预览 Request 优先级 > 抓拍 AI。** IFE 写 RAW 环用独立 DMA 客户端，不被 NPU 堵住 QBUF。  
2. AI 任务队列深度 1：连按快门则合并或丢旧的（产品定）。  
3. 录像中夜景抓拍：限 NPU 带宽，或只做单帧 HW snapshot。  
4. 模型未加载（冷启动）：still 回退纯 HW BPS，Result 带 vendor tag `ai_bypass=1`。

时间线：

```
t=0     快门
t=0–30ms  预览照常；UI 快门动画
t=0–50ms  选帧、对齐
t=50–400ms NPU infer
t=400ms+  IPE 收尾 + JPEG
预览全程 30fps 不中断
```

### 3.3 内存与算力隔离（QoS）

NPU 跑 12MP RAW 卷积会打满 DDR。不隔离就会：预览掉帧、tracking 丢、encoder 花屏。

| 机制 | 用法 |
|------|------|
| **独立 analysis/RAW 缓冲池** | 与 preview YUV 池分开，避免 HAL buffer 饿死 |
| **缩小网络输入** | 12MP → 在 Bayer 上 2×2 bin 再进网络，输出再引导融合 |
| **NPU DVFS cap** | 预览掉帧时压 NPU 频率，而不是压 IFE |
| **带宽 QoS / 业务优先级** | SoC 若有 camera traffic class：IFE/VPU > GPU > NPU still |
| **DDR 忙则延期 AI** | 热路径只设 flag，下一帧再跑 |

Zero-copy：NPU 直接 import IFE 的 RAW dma-buf（需 **cache/SMMU 属性** 与 ISP 写出一致）。若网络要 planar 16-bit，在 **IFE 或小 DSP 做 unpack**，仍避免 CPU。

功耗：夜景是 burst，允许短时超 2W，但必须能被眼镜热状态机 **T2 直接取消 NPU**（见 [`03-smart-glasses-power-thermal.md`](./03-smart-glasses-power-thermal.md)）。

---

## 4. 性能优化与边界

| 问题 | 处理 |
|------|------|
| 鬼影 | 对齐失败则少融合帧或回退单帧 HW |
| 闪烁 | 多帧 AE 变化 → 只融曝光相近帧；或先对齐曝光 |
| 掉帧恢复 | NPU hang watchdog 100ms → kill infer、HW JPEG、上报 |
| Tiling | 超分按 tile，重叠 halo，降低 peak 内存 |
| 版本 | 模型文件与 ISP IQ 绑定；OTA 同包，避免“新模型 + 旧 BLC” |

调试：预览正常、JPEG 差 → 查训练域 vs BLC；JPEG 好、预览差 → 别怪 NPU，查快路径 IQ。

---

## 5. 跨团队落地

| 团队 | 契约 |
|------|------|
| 算法 | 输入张量 layout（packed RAW10 vs 16-bit）、黑电平是否已减、Bayer pattern、色温作为 side feature |
| IQ | 快路径与 AI 收尾 CCM 必须同一套，否则预览/成片色不一致 |
| 驱动 | RAW 环深度、fd 生命周期、NPU 与 IFE 并发的 SMMU 上下文 |
| 系统 | QoS 表、热取消 AI 的 API |
| 产品 | 夜景等待文案；失败回退是否对用户可见 |
| 评测 | 预览 FPS、抓拍 PSNR/SSIM、DDR 占用、皮肤温；**不能只报网络 PSNR** |

标定：LSC/BLC 仍归 ISP；网络不要学镜头阴影，否则换模组必坏。产线只出传统 OTP + 通用模型；SKU 特化用微调包。

**收口金句：** 混合架构的成功标准是 **预览帧率在 AI 抓拍时不掉、3A 不依赖 NPU、NPU 挂了还能出片**。画质增益是附加条件，不是系统正确性本身。
