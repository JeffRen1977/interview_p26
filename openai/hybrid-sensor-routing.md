# 设计 10 万台智能相机的混合路由与端云协同系统

> **核心考点：** 全量视频上云的带宽与 GPU 成本不可承受 → **端云协同 + 置信度级联**  
> **必须覆盖：** 端侧 1B–3B INT4 + PagedAttention；Confidence 阈值路由；5G **增量上传 Embedding** 触发云端大模型  
> **关联 Demo：** [hybrid_sensor_routing.py](./hybrid_sensor_routing.py)  
> **关联：** [../LLM/paged_attention.py](../LLM/paged_attention.py) · [../docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 假设 | 设计影响 |
|------|------|----------|
| 规模 | **10 万** 相机，1080p@10–15fps | 禁止默认全量视频上云 |
| 任务 | 安防/工业：检测、动作、异常、可选 OCR/问答 | 简单视觉在端；复杂推理在云 |
| SLA | 本地告警 &lt; 100ms；云端复核秒级可接受 | 热路径不上大模型 |
| 成本 | 云端调用 &lt; **5%** 帧 / &lt; $Y 每设备每月 | 阈值与采样是一等公民 |
| 隐私 | 默认不出原图 | 优先 embedding / metadata |
| 网络 | 5G/有线混布，链路不稳 | 增量、可重试、本地 ring buffer |

**开场金句：**

> 10 万路 1080p 不是「加带宽」能解的题。架构中心是 **Hybrid Routing**：端侧小模型吃掉 95%+ 帧；只有 **低置信或高阶推理** 才把 **紧凑 Embedding（而非原视频）** 经 5G 送给云端 gpt-4o 类大模型。Embedded 的本质是 **在约束下做路由决策**。

---

## 1. 为什么不能全量上云（白板算账）

```
10⁵ cameras × 1080p@15fps × ~4 Mbps 压缩 ≈ 400 Tbps 量级（不可部署）
即使 1% 抽帧上传仍极贵

云端 VLM：每帧/每事件数美分～数毛 → 月成本爆炸
```

| 上云内容 | 约大小 | 相对成本 |
|----------|--------|----------|
| 1080p 视频流 | Mb/s | ❌ 禁止默认 |
| 5s clip | MB | 稀有复核 |
| 低分 crop | 10–100KB | 中 |
| **Embedding (512–1536-d)** | **~KB** | ✅ 级联默认 |
| 事件 JSON | &lt;1KB | ✅ 常开 |

---

## 2. 总体架构

```
┌──────────────────── Edge Camera / Gateway ────────────────────┐
│ Camera ──► ISP ──► dma-buf ──► NPU                             │
│                         │                                      │
│              1B–3B INT4 小模型 (detect / action / VLM-lite)     │
│              + PagedAttention KV (本地多模态对话/跟踪上下文)     │
│                         │                                      │
│                   confidence C + complexity flag               │
│                         │                                      │
│         ┌───────────────┼───────────────┐                      │
│         ▼               ▼               ▼                      │
│    C ≥ θ_high     θ_low≤C<θ_high     C<θ_low 或 complex       │
│    本地闭环         上传 embedding      丢弃 / ring buffer      │
│    + 事件 JSON      (+ 可选 crop)       待抽查                  │
└─────────────┬─────────────────┬────────────────────────────────┘
              │ MQTT/QUIC       │ 5G 增量 embedding
              ▼                 ▼
┌──────────────── Control Plane ──┐    ┌────── Cloud AI ──────────┐
│ Device Shadow (θ, model ver)    │    │ Router / Admission       │
│ OTA + A/B                       │───►│ gpt-4o / 大 VLM          │
│ Fleet metrics → 调阈值         │    │ 结果回写 + 阈值建议      │
└─────────────────────────────────┘    └──────────────────────────┘
```

---

## 3. 多级路由（Hybrid Routing）

### 3.1 端侧小模型（1B–3B，INT4）

| 能力 | 说明 |
|------|------|
| 目标检测 / 分割 | 人、车、入侵区域 |
| 动作 / 行为 | 摔倒、打斗、徘徊（短时序） |
| 场景分类 | 正常营业 vs 异常 |
| 轻量 VLM | 短 caption / 简单问答（可选） |

**INT4 量化：**

- 权重大幅缩内存与带宽，适配 NPU  
- 注意 calibration；关键类别做 per-channel 保护  
- 详见端侧部署文档

**PagedAttention（本地 KV）：**

- 多轮「跟这个人」/ 短视频问答会在端侧维护 KV  
- 设备 RAM 极紧 → **分页块 + 复用**，避免 `torch.cat` 式连续扩容  
- 实现对照：[../LLM/paged_attention.py](../LLM/paged_attention.py)

> 面试金句：端侧不只是 CNN 检测器；一旦有时序/对话，**KV 内存**就变成一等瓶颈，PagedAttention 从服务端搬到边缘同样成立。

### 3.2 路由策略（三档）

设本地输出置信度 $C \in [0,1]$，以及复杂度标记 `needs_reasoning`（OCR 长文本、多物体关系、开放问答等）。

| 条件 | 动作 | 上云载荷 |
|------|------|----------|
| $C \ge \theta_{high}$ 且 ¬complex | **本地闭环** | 事件 JSON（可选） |
| $\theta_{low} \le C < \theta_{high}$ | **级联** | **Embedding** ± 低分 crop |
| $C < \theta_{low}$ | 丢弃或写入本地 ring buffer | 默认不上云 |
| `needs_reasoning=True` | **强制级联** | Embedding + 必要 crop/文本 |

典型出发值（需校准）：$\theta_{high}=0.90$，$\theta_{low}=0.55$。

### 3.3 置信度必须校准（Calibration）

原始 softmax ≠ 可靠概率。生产用：

- Temperature scaling / vector scaling on-device  
- 云端抽检反馈 → **联邦式调 θ**（不下发原始视频）  
- 按场景分 θ（夜间/雨天更低 θ_high → 更多上云）

---

## 4. 级联触发与 5G 增量 Embedding

### 4.1 为什么传 Embedding 而不是帧

```
Frame 1080p JPEG crop ~ 50–200KB
Embedding 1024 × float16 ~ 2KB
→ 带宽差 25–100×；云端还可批处理 embedding
```

**增量：**

- 同一 track_id 只在 **keyframe / 置信度变化 / 场景切换** 时上传  
- 运动矢量或特征差低于 ε → skip  
- 断网：本地队列；恢复后按优先级（告警 &gt; 统计）补传

### 4.2 云端大模型职责

| 端侧 | 云端 gpt-4o / 大 VLM |
|------|----------------------|
| 高频、低延迟感知 | 低频、高阶推理 |
| 「有人翻墙」高置信 | 「是否违规施工 + 写工单」 |
| 短 OCR | 长文档 / 多牌对照 |
| 过滤 95%+ | 复核模糊 case |

云端输入建议：**embedding + 结构化 metadata**（时间、GPS、track、端侧 top-label）；仅当大模型要求时再拉 crop。

### 4.3 回写闭环

```
Cloud decision → 
  ① 告警/工单
  ② 建议调 θ（device shadow）
  ③ 难例 embedding 进入蒸馏集 → 下次 OTA 改进端侧模型
```

---

## 5. 控制平面（10 万设备）

| 组件 | 作用 |
|------|------|
| **Device Shadow** | 期望：`model_ver`, `θ_high/low`, `cascade_enabled`, `privacy_mode` |
| **OTA / A/B** | 灰度 INT4 模型；一键回滚 |
| **Command bus** | MQTT/QUIC：临时提高灵敏度、请求补传 clip |
| **Fleet telemetry** | cascade rate、带宽、误报；自动寻优 θ |
| **租户隔离** | `camera_id → tenant`；embedding 索引分 namespace |

**容量粗算：**

```
稳态 cascade rate r = 3%
每路上云 embedding QPS ≈ 15fps × 0.03 = 0.45/s
10⁵ 路 ≈ 4.5×10⁴ embedding/s → 需水平扩展 embedding gateway + 限流
```

与 [llm-rate-limiter.md](./llm-rate-limiter.md) 结合：云端按 **tenant TPM/事件配额** 限级联。

---

## 6. 隐私与安全

- 默认 **embedding-only**；原图需权限 + 审计  
- 端侧加密存储 ring buffer；密钥硬件隔离  
- 出站 certificate pin；防设备被劫持打成肉鸡（对照沙箱 default-deny 思想）  
- 多租户向量库强制 tenant filter（对照语义缓存隔离）

---

## 7. 与其它系统的衔接

| 系统 | 关系 |
|------|------|
| 语音助手 | 同一套「端侧过滤 + 云端重模型」哲学 |
| PagedAttention | 边缘 KV 与云端推理显存治理同构 |
| 限流 | 级联请求占云端 TPM |
| 语义缓存 | 云端对相似 embedding 事件可缓存答复 |

---

## 8. 10x / 100x Scaling Drill

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1×（1 万路） | 单区云端 VLM | 提 θ_high；加本地模型 |
| 10×（10 万） | **级联风暴**（天气/活动） | 自适应 θ；tenant 配额；只传 embedding |
| 100× | OTA + shadow 配置扇出 | 分层控制面；区域聚合器 |

> 「100x 时不要先买带宽。先把 cascade rate 从 10% 压到 2%，效果等于 5× 降本。」

---

## 9. 面试口述 3 分钟版

1. **算账：** 全量视频不可能；目标 cascade &lt; 5%。  
2. **端侧：** 1B–3B INT4 + PagedAttention 做检测/动作/过滤。  
3. **路由：** 高置信本地闭环；模糊或高阶推理 → **5G 上传 embedding** → 云端大模型。  
4. **控制面：** Shadow 调 θ、OTA、租户配额。  
5. **闭环：** 云端难例蒸馏回端侧，持续降上云率。

---

## 10. 参考实现

```bash
cd openai
python3 hybrid_sensor_routing.py
```

模拟：三档路由、强制 reasoning 级联、embedding 带宽 vs 视频、fleet cascade rate。

---

## 11. 追问清单（自测）

- [ ] 为什么默认传 embedding 不传 JPEG？  
- [ ] θ_high / θ_low 如何校准？  
- [ ] 端侧为何需要 PagedAttention？  
- [ ] 级联风暴（演唱会/暴雨）怎么降载？  
- [ ] 10 万设备如何安全 OTA 与回滚？  
- [ ] 隐私模式下 ring buffer 谁有权调原视频？  
- [ ] 云端限流用 RPM 够不够？
