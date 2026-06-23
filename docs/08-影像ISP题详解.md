# 08 - 影像 / ISP 题详解

对应 [02-PICO面试题库.md](./02-PICO面试题库.md)「影像 / ISP」部分。这是 **Imaging Systems & Human Engineering** 岗位的核心专业考点，贯穿 PICO **capture-to-display** 全链路。

> **GitHub 阅读：** 公式使用 `$...$` / `$$...$$` 格式。

---

## 总览：Capture-to-Display 链路

```
Sensor (RAW Bayer)
    ↓ 模拟前端 / ADC
ISP Pipeline（传统或 AI-ISP）
    ↓ YUV / RGB
计算摄影增强（去噪、HDR、超分、去模糊）
    ↓
CV 感知（深度、分割、场景理解）
    ↓ [可选] 云端 VLM
显示前处理（色彩映射、畸变校正、MR 虚实对齐）
    ↓
Display（sRGB/P3，gamma，透镜光学）
    ↓
人眼感知（MOS 主观评测）
```

**面试总纲：** PICO 不只关心「算法精度」，更关心整条链路的**画质、延迟、功耗、一致性**。

---

## 一、ISP Pipeline

### Q1：RAW → demosaic → NR → sharpen → tone mapping 流程是什么？

#### 1.1 为什么从 RAW 开始？

相机 Sensor 输出的是 **Bayer RAW**（每个像素只有一种颜色：R/G/B）：

```
Bayer 排列示例（RGGB）：
R G R G ...
G B G B ...
```

RAW 保留了传感器记录的**线性光强**、更宽的动态范围和更多原始信息，尚未经过不可逆的色调/色彩压缩。ISP 的任务是把 RAW 转成对人眼友好的图像。

#### 1.2 典型 ISP Pipeline（按顺序）

| 阶段 | 作用 | 关键点 |
|------|------|--------|
| **Black Level Correction (BLC)** | 减去暗电流偏移 | 不同 ISO/温度下 black level 不同 |
| **Lens Shading Correction (LSC)** | 补偿镜头渐晕 | 四角变暗校正 |
| **坏点校正 (DPC)** | 去除 hot/dead pixel | 静态表 + 动态检测 |
| **Demosaic（去马赛克）** | Bayer → 全 RGB | 插值算法：双线性、Malvar、AI demosaic |
| **Auto White Balance (AWB)** | 估计光源色温，调整 R/B gain | Gray World、AWB stats |
| **Auto Exposure (AE)** | 控制亮度 | 统计直方图，反馈 sensor 曝光 |
| **Noise Reduction (NR)** | 去噪 | 空域（双边滤波）、频域、时域（多帧） |
| **Sharpen（锐化）** | 增强边缘 | Unsharp mask，控制 overshoot 避免 halos |
| **Tone Mapping / Gamma** | 线性光强 → 显示亮度 | 全局/局部 tone mapping，gamma 编码 |
| **Color Correction Matrix (CCM)** | 传感器 RGB → 标准色彩空间 | 3×3 矩阵，标定获得 |
| **可选：3DNR / HDR fusion** | 多帧合成 | 运动对齐 + 权重融合 |

#### 1.3 Demosaic 详解

**问题：** 每个像素缺 2/3 颜色信息，需插值恢复。

**双线性（基础）：** 用邻域同色像素平均 — 易有 zipper artifact（边缘彩纹）。

**高级方法：** 沿边缘方向插值（梯度自适应）、Malvar-He-Cutler、**AI demosaic**（小型 CNN 在 RAW patch 上预测 RGB）。

**面试要点：** demosaic 错误会传播到后续所有步骤；AI-ISP 常在 RAW 域早期介入以保留信息。

#### 1.4 NR（降噪）

| 类型 | 原理 | 适用 |
|------|------|------|
| **空域 NR** | 双边滤波、Non-Local Means、BM3D | 单帧 |
| **频域 NR** | 小波阈值 | 特定噪声模型 |
| **时域 NR（3DNR）** | 多帧对齐后加权平均 | 视频/连拍，需运动补偿 |
| **AI NR** | 深度网络学习噪声分布 | 低光、高 ISO |

**权衡：** 降噪 ↔ 细节保留；强度过大 → 涂抹感（waxiness）；需与 sharpen 配合。

#### 1.5 Sharpen（锐化）

**Unsharp Mask：**

$$
I_{sharp} = I + \alpha (I - G_\sigma * I)
$$

- $G_\sigma * I$：模糊版
- $\alpha$：锐化强度

**风险：** 噪声也被放大；NR 通常在 sharpen **之前**；halos 出现在高对比边缘。

#### 1.6 Tone Mapping

**目的：** 传感器线性动态范围（如 12–14 bit）映射到显示 8 bit，同时保留主观层次感。

| 类型 | 说明 |
|------|------|
| **全局 tone mapping** | 整图一条曲线（gamma、对数） |
| **局部 tone mapping** | 分块自适应，对比度更好但可能 halo |
| **HDR tone mapping** | 多曝光帧融合后压缩动态范围 |

**Gamma 编码：**

$$
V_{out} = V_{in}^{1/\gamma}, \quad \gamma \approx 2.2 \text{（sRGB）}
$$

显示端再做 gamma 解码，补偿人眼非线性感知。

#### 1.7 PICO / MR 特殊考量

- **See-through 透视：** ISP 延迟要低，多帧 NR/HDR 受限  
- **色彩一致性：** 虚拟内容与现实场景色彩匹配，AWB/CCM 要稳  
- **双相机：** 立体 ISP 参数需同步，避免深度误差  

---

## 二、计算摄影（Computational Photography）

### Q2：去噪、HDR 合成、超分、去模糊的原理？

#### 2.1 图像去噪

**噪声来源：** 光子散粒噪声（与信号相关）、读出噪声、热噪声；低光高 ISO 最明显。

**传统方法：**

| 方法 | 思想 |
|------|------|
| 高斯滤波 | 空域平滑，模糊边缘 |
| 双边滤波 | 空间近 + 值相似才平均，保边 |
| NL-Means | 找相似 patch 平均 |
| BM3D | 块匹配 + 3D 变换域阈值，经典强 baseline |

**深度学习方法：**

$$
\mathcal{L} = \| f_\theta(I_{noisy}) - I_{clean} \|_2^2 \quad \text{或} \quad \mathcal{L}_{perceptual}
$$

代表：DnCNN、FFDNet、NAFNet；RAW 域去噪：Noise2Noise、Noise2Void（自监督）。

**端侧：** 轻量 U-Net / NAFNet + INT8；多帧时域融合在低光 MR 透视中常用。

---

#### 2.2 HDR 合成（High Dynamic Range）

**问题：** 单次曝光无法同时保留亮部和暗部细节。

**多帧 HDR 流程：**

```
1. 拍摄多张不同曝光（-2EV, 0, +2EV）或单帧分区
2. 对齐（运动补偿，避免 ghosting）
3. 融合权重图：W_i(x,y) 根据饱和度、对比度、亮度
4. 合成 radiance map：R = Σ W_i · I_i
5. Tone mapping → 8-bit 显示
```

**融合权重（Debevec 经典思想）：** 每张曝光在其「最佳响应区间」权重大，过曝/欠曝区域权重低。

**Ghosting：** 运动物体在不同曝光帧位置不同 → 需检测运动区域，单帧 fallback 或 exclusion mask。

**AI HDR：** 网络直接从短序列预测 HDR 或 tone-mapped 结果（如 Deep HDR、Kalantari）。

**PICO 场景：** MR 透视中动态范围大（窗外亮、室内暗），HDR + 局部 tone mapping 是画质关键。

---

#### 2.3 超分辨率（Super-Resolution）

**问题：** 从低分辨率（LR）恢复高分辨率（HR）细节。

**单帧超分：**

$$
\hat{I}_{HR} = f_\theta(I_{LR})
$$

- 早期：SRCNN、ESRGAN（感知损失 + 对抗训练）
- 实用：Real-ESRGAN、SwinIR
- 损失：L1/L2（保真）+ perceptual + GAN（视觉锐利）

**多帧超分：** 亚像素对齐后融合，利用帧间互补信息。

**视频超分：** 时序一致性重要，避免 flickering。

**端侧权衡：** 4× 超分计算量大；MR 中可能只做 2× 或仅对 ROI（注视区域）超分。

**与 sharpen 区别：** 超分是**合成新频率**；锐化是**增强已有边缘**。过度超分可能产生幻觉纹理。

---

#### 2.4 去模糊（Deblurring）

**成像模型：**

$$
B = K * I + N
$$

- $B$：模糊图，$I$：清晰图，$K$：模糊核（point spread function），$N$：噪声

**分类：**

| 类型 | 说明 |
|------|------|
| **运动模糊** | 曝光期间相机/物体运动，核为运动轨迹 |
| **散焦模糊** | 离焦，核近似圆盘 |
| **盲去模糊** | 核未知，同时估计 $K$ 和 $I$ |

**传统：** 维纳滤波、Richardson-Lucy 迭代反卷积。

**深度：** DeblurGAN、MPRNet、Restormer；端到端直接 $B \to \hat{I}$。

**MR 难点：** 头部快速转动 → 运动模糊；去模糊需与 **rolling shutter** 校正联合考虑。

---

## 三、AI-ISP

### Q3：RAW 域网络 vs RGB 域网络的 trade-off？

#### 3.1 传统 ISP vs AI-ISP

| | 传统 ISP | AI-ISP |
|---|----------|--------|
| **实现** | 固定算法 + 调参 | 神经网络替代或增强部分模块 |
| **灵活性** | 规则明确，可解释 | 数据驱动，端到端优化 |
| **算力** | 低，可硬件化 | 需 NPU/GPU |
| **调优** | 工程师调参 | 需要训练数据和标注 |

**AI-ISP 常见形态：**
- 替换单个模块：AI demosaic、AI NR、AI AWB  
- 端到端：RAW in → RGB out（如 AWNet、KBNet）  
- 联合优化：ISP + 下游 CV 任务多任务训练  

#### 3.2 RAW 域处理

**优点：**
- 保留传感器**完整动态范围**和线性光强信息  
- 避免传统 ISP 各步骤的**累积误差**和不可逆损失  
- 与 Bayer 噪声模型更一致，低光去噪潜力大  
- 可多任务：同一 RAW 特征同时服务 NR + 深度 + 检测  

**缺点：**
- 需处理 **Bayer pattern**（4 通道 pack 或 demosaic 前卷积）  
- 训练数据需 **RAW 标注**（难获取，常需 paired RAW-clean RGB 或合成噪声）  
- 不同 sensor 间 **域偏移**大，泛化需多 sensor 训练或 adapter  
- 与硬件 ISP 管线集成复杂，延迟需严格控制  

#### 3.3 RGB 域处理

**优点：**
- 数据丰富（公开数据集都是 RGB）  
- 网络结构通用，预训练模型多  
- 与现有 CV pipeline 无缝对接  
- 调试直观，PSNR/SSIM 易评估  

**缺点：**
- 传统 ISP 已做 tone mapping/gamma，**信息已压缩**，恢复空间有限  
- 难以纠正 ISP 上游错误（如 AWB 偏色）  
- 增强任务（HDR、去噪）上限受限于 8-bit 输入  

#### 3.4 选型决策（面试模板）

```
IF 有 RAW 访问权 + 低光/HDR 是核心痛点 + 算力够
   → RAW 域 AI-ISP 或 hybrid（传统 AE/AWB + AI NR）

IF 只能拿到 YUV/RGB + 快速上线
   → RGB 域网络增强

IF 量产一致性要求高
   → 传统 ISP 保底 + AI 增强关键模块（渐进式）
```

#### 3.5 Hybrid 方案（工业界常见）

```
Sensor RAW
  → 硬件 ISP（AE/AWB/Demosaic 基础版，低延迟）
  → AI NR / AI sharpen（NPU，可微调）
  → RGB
  → CV / 显示
```

**PICO MR 透视：** 往往 hybrid — 硬件 ISP 保实时性，AI 做 selective enhancement（低光区域、局部对比度）。

---

## 四、相机（Camera）

### Q4：标定、畸变校正、rolling shutter、多相机同步？

#### 4.1 相机标定（Calibration）

**目的：** 求相机**内参**和**外参**，建立 3D 世界点与 2D 像素的关系。

**针孔模型：**

$$
s \begin{bmatrix} u \\ v \\ 1 \end{bmatrix}
= K [R|t] \begin{bmatrix} X \\ Y \\ Z \\ 1 \end{bmatrix}
$$

**内参矩阵 $K$：**

$$
K = \begin{bmatrix} f_x & 0 & c_x \\ 0 & f_y & c_y \\ 0 & 0 & 1 \end{bmatrix}
$$

- $f_x, f_y$：焦距（像素单位）  
- $(c_x, c_y)$：主点  

**畸变模型（Brown-Conrady）：**

$$
\begin{aligned}
x_{dist} &= x(1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + 2p_1 xy + p_2(r^2+2x^2) \\
y_{dist} &= y(1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + p_1(r^2+2y^2) + 2p_2 xy
\end{aligned}
$$

$r^2 = x^2 + y^2$；$k_1,k_2,k_3$ 径向畸变，$p_1,p_2$ 切向畸变。

**标定方法：** 张正友标定法 — 多张棋盘格图像，优化重投影误差。

**PICO 应用：** MR 透视需精确知道相机内外参，才能做虚实对齐；每个头显出厂需**个体标定**或参考标定 + 微调。

---

#### 4.2 畸变校正（Undistortion）

**步骤：**
1. 标定得到 $K$ 和畸变系数  
2. 对输出图像每个像素 $(u,v)$，反查畸变前坐标 $(u',v')$  
3. 双线性插值采样（见 [03-算法数学公式.md](./03-算法数学公式.md)）  

**优化：** 预计算 **remap 查找表（LUT）**，实时只做查表 + 插值。

**MR 特殊：** 头显显示也有透镜畸变 → **显示畸变校正**与**相机畸变校正**需分别处理，最终在统一坐标系融合。

---

#### 4.3 Rolling Shutter（卷帘快门）

**原理：** 逐行曝光/读取，不同行的采集时间不同。

**问题：** 快速运动时，图像产生**几何畸变**（倾斜、果冻效应）。

```
全局快门：整帧同时曝光 → 无果冻
卷帘快门：第1行先曝光，第N行后曝光 → 运动物体被「拉长/倾斜」
```

**影响：**
- 视觉 SLAM / VIO 位姿估计误差  
- 多帧 HDR/对齐失败  
- 深度估计（立体匹配）在运动时不准  

**校正思路：**
- 利用 IMU 估计每行曝光时刻的相机姿态，做行级 warp  
- 与去模糊联合建模  
- 硬件选型：全局快门 sensor（成本高）  

**PICO：** 头显快速转头时 rolling shutter 明显；MR 透视和 SLAM 必须考虑。

---

#### 4.4 多相机同步

**为什么需要：** PICO MR 通常有多个前置相机（立体、广角），ISP/CV 要求帧**时间对齐**。

| 同步层级 | 说明 |
|----------|------|
| **硬件同步** | 共享 FSIN（frame sync）信号，同时曝光 |
| **时间戳对齐** | 每帧打 PTS/硬件 timestamp |
| **软件对齐** | 时间戳插值，选最近邻或丢帧 |

**立体匹配前提：** 左右图同步误差 < 1ms（运动场景），否则深度图错误。

**面试要点：**
- 曝光/增益（3A）是否独立还是联动  
- 工厂标定：外参 $R,t$ 左右相机相对位姿  
- 温漂：长期使用后外参可能漂移，需在线微调  

---

## 五、人眼视觉（Human Vision）

### Q5：色彩空间、sRGB/P3、gamma、MOS 主观评测？

#### 5.1 色彩空间

| 空间 | 说明 | 用途 |
|------|------|------|
| **CIE XYZ** | 设备无关，基于人眼三刺激值 | 色彩科学基准 |
| **sRGB** | 标准 RGB，色域较小 | 网页、大多数显示器 |
| **Display P3** | 更广色域（苹果/PICO 等） | 更鲜艳，接近人眼可感知 |
| **BT.2020** | 超广色域 | HDR 视频 |
| **YUV / YCbCr** | 亮度 + 色度分离 | ISP 内部、视频编码 |

**色域示意（面试口述）：** P3 比 sRGB 能表现更饱和的红/绿，MR 虚实融合时若色域不一致会「假」。

**转换：** 线性 RGB → 乘 **CCM** → 非线性 gamma 编码 → 目标色域。

#### 5.2 Gamma

**原因：** 人眼对暗部更敏感；显示设备非线性；用 gamma 编码让暗部占用更多 bit 深度。

**sRGB 近似：**

$$
C_{out} = \begin{cases}
12.92 C_{in} & C_{in} \le 0.0031308 \\
1.055 C_{in}^{1/2.4} - 0.055 & \text{otherwise}
\end{cases}
$$

**线性光 ↔ 显示值** 必须在增强算法中分清：  
- 去噪、HDR 融合宜在**线性域**  
- 显示前再做 gamma  

**错误示范：** 在 gamma 编码后的 sRGB 上做线性平均 → 亮度偏差。

#### 5.3 客观 vs 主观评测

| 类型 | 指标 | 局限 |
|------|------|------|
| **PSNR** | 像素 MSE | 与人眼感知不一致 |
| **SSIM** | 结构相似性 | 仍不能完全代表主观 |
| **LPIPS** | 深度学习感知距离 | 更接近人眼，需参考图 |
| **MOS** | Mean Opinion Score，多人打分 1–5 | 金标准但成本高 |

**MOS 测试流程：**
1. 招募被试（≥15 人常见）  
2. 随机对比原图/增强图（双盲）  
3. 维度：清晰度、噪声、色彩自然度、舒适度、整体偏好  
4. 统计均值和置信区间  

**PICO 特殊：** MR 透视要评 **晕动症（comfort）** 和 **虚实色彩一致性**，不单看 PSNR。

#### 5.4 人眼感知与算法设计

- **Weber-Fechner定律：** 感知与 log(亮度) 相关 → tone mapping 设计依据  
- **对比度敏感度：** 中频细节最敏感 → sharpen 频段选择  
- **色适应：** AWB 错误会导致整体偏色，人眼会部分适应但 MR 虚实对比下更明显  

---

## 六、端云协同

### Q6：哪些任务放端、哪些放云？隐私/延迟怎么权衡？

#### 6.1 决策框架

| 因素 | 倾向端侧 | 倾向云端 |
|------|----------|----------|
| **延迟** | <30ms 实时（透视、追踪） | 可异步（场景语义描述） |
| **隐私** | 原始图像不出设备 | 可上传脱敏特征 |
| **算力** | 轻量 CNN、ISP | 大 VLM、大规模检索 |
| **离线** | 无网可用 | 需联网 |
| **带宽** | 上传贵/慢 | WiFi 6 可传压缩特征 |

#### 6.2 PICO 典型任务划分

| 任务 | 端侧 | 云端 | 说明 |
|------|------|------|------|
| ISP / 实时增强 | ✅ | ❌ | 延迟敏感 |
| 手部/面部追踪 | ✅ | ❌ | MTP < 20ms |
| 深度估计 / 透视 | ✅ | ❌ | 每帧必需 |
| 物体检测（基础） | ✅ | ❌ | 轻量模型 |
| 场景语义描述 | 触发 | ✅ VLM | 用户问「这是什么」时 |
| 地图/物体库检索 | 本地缓存 | ✅ 更新 | 特征向量上传，非原图 |
| 模型 OTA 更新 | 下载 | ✅ 托管 | 非实时 |
| 训练数据回流 | 脱敏特征 | ✅ | 需用户授权 |

#### 6.3 隐私设计

```
原始相机图像 ──永不离开设备──┐
                            ├→ 端侧 ISP + CV
脱敏选项：                  │
  - 上传语义标签（「厨房」）  │
  - 上传 embedding 向量      │
  - 上传模糊/裁剪 ROI         │
  - 差分隐私噪声              │
```

**面试加分：** 引用 **联邦学习**、**on-device personalization**（本地微调小 adapter，权重不上传）。

#### 6.4 延迟预算与端云协同架构

```
端侧（每帧，<20ms）：
  相机 → ISP → 深度/分割 → 显示增强 → 渲染

云端（异步，100ms~秒级）：
  端侧上传：场景 embedding + 用户 query
  云端 VLM 推理 → 返回文本/结构化结果
  端侧展示 UI overlay
```

**渐进式：** 端侧先出粗结果（「有桌子」），云端返回细结果（「木质餐桌，上有笔记本电脑」）。

#### 6.5 失败与降级

| 场景 | 策略 |
|------|------|
| 无网络 | 仅端侧功能，隐藏云特性 |
| 云端超时 | 显示端侧结果，静默重试 |
| 隐私模式 | 用户开关，禁止任何上传 |

---

## 七、PICO Imaging 岗综合题示例

**题：** 设计 MR 透视的低光增强方案，从 RAW 到显示。

**参考回答结构：**

1. **约束：** 18ms 预算、骁龙 XR、卷帘快门  
2. **采集：** 短曝光多帧或单帧 RAW，IMU 辅助对齐  
3. **ISP：** 硬件 demosaic + AWB；AI RAW NR（INT8）  
4. **HDR：** 2–3 帧对齐融合（运动区域 mask）  
5. **Tone mapping：** 局部曲线，保窗外细节  
6. **色彩：** CCM 对齐到 P3 显示色域  
7. **评测：** PSNR + MOS（清晰度/舒适度/色彩自然度）  
8. **端云：** 增强全端侧；场景描述可选上云  

---

## 八、速记表（考前 5 分钟）

| 主题 | 一句话 |
|------|--------|
| ISP 流程 | BLC → LSC → demosaic → AWB/AE → NR → sharpen → tone → CCM |
| Demosaic | Bayer 插值变全 RGB，AI 可减彩纹 |
| NR | 空域/时域/AI；在 sharpen 前 |
| HDR | 多曝光对齐融合 + tone mapping，注意 ghosting |
| 超分 | LR→HR 合成细节；注意幻觉纹理 |
| 去模糊 | $B=K*I+N$；运动模糊 + rolling shutter 相关 |
| RAW 域 AI | 信息多、难训练、sensor 泛化难 |
| RGB 域 AI | 数据多、上限低、集成简单 |
| 标定 | $K$, 畸变系数, $[R|t]$，张正友法 |
| Rolling shutter | 逐行曝光，运动畸变，IMU 校正 |
| 多相机 sync | 硬件 FSIN + 时间戳 + 立体外参 |
| sRGB vs P3 | P3 色域更广，MR 需虚实一致 |
| Gamma | 线性域做算法，显示前编码 |
| MOS | 主观金标准，MR 加舒适度维度 |
| 端云 | 实时 ISP/追踪在端；VLM 异步在云 |

---

## 九、与项目结合的回答示例

> 在 `[MR 透视 / 低光增强]` 项目中，我从 Sensor RAW 入手，采用 hybrid ISP：硬件完成 demosaic 和 AWB，AI NR 在 RAW 域用 `[模型]` 运行在 NPU 上，INT8 推理 `[X]` ms。相比 RGB 域方案 PSNR 提升 `[Y]` dB，MOS 清晰度得分提高 `[Z]`。同时做了多相机时间戳对齐和畸变 LUT 校正，保证立体深度误差 < `[N]`%。云端仅上传场景 embedding 用于 `[功能]`，原始图像不出设备。
