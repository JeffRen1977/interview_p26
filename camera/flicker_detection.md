# 频闪检测（Flicker Detection / Anti-Banding）

> 配套：[3A.md](./3A.md)（AE 拆 `(t, again, dgain)` 后的 antibanding 网格）· [sensor.md](./sensor.md)（曝光 / 行时 / VTS）· [../XR/Meta_Camera/code/fixed_point.md](../XR/Meta_Camera/code/fixed_point.md)（Q 格式、乘加圆整）

在现代相机与移动影像系统（ISP / 3A 管线）中，频闪检测主要用于解决两类伪影：

1. **Rolling Shutter banding**：交流电驱动光源（50 Hz / 60 Hz → 光照 100 Hz / 120 Hz）在逐行曝光上留下垂直方向明暗条纹。
2. **帧间亮度 / 色温跳跃**：曝光窗口与光源相位相对滑动，或高频 PWM LED / 显示屏占空比与帧率不对齐。

主流算法分两类：**基于图像传感器数据（In-Sensor / ISP-based）**，以及 **基于专用硬件传感器（Dedicated Flicker Sensor）**。

---

## 0. 物理成因（面试先画这一张）

交流市电是正弦，灯具发光近似全波整流，**光强频率 = 2 × 电网频率**：

$$
T_{50} = \frac{1}{100\,\mathrm{Hz}} = 10\,\mathrm{ms},\qquad
T_{60} = \frac{1}{120\,\mathrm{Hz}} \approx 8.333\,\mathrm{ms}
$$

Rolling Shutter 每行积分窗口沿时间轴错开一个行时 \(T_{\mathrm{line}}\)。若曝光时间 \(t_{\mathrm{exp}}\) **不是** \(T_{50}\) / \(T_{60}\) 的整数倍，各行积分到的光通量不同，图像垂直方向出现空间周期

$$
\lambda_{\mathrm{rows}} = \frac{T_{\mathrm{flicker}}}{T_{\mathrm{line}}}
$$

的明暗条纹。\(T_{\mathrm{line}}\) 由 sensor 行读出（HTS / line length）决定，和当前 mode 的 fps、H-blank 绑在一起。

**Anti-banding 的本质不是“去条纹滤波”，而是把 \(t_{\mathrm{exp}}\) 锁到半周期网格上**，让每一行积分完整个整数个闪烁周期，行间光通量一致。检测负责回答：当前是 50 Hz、60 Hz、混合、还是无频闪 / 高频 PWM。

---

## 1. 基于图像统计与 Rolling Shutter 效应（ISP 软件端）

利用 RS 逐行曝光：当 \(t_{\mathrm{exp}}\) 不是闪烁周期整数倍时，行均值在垂直方向呈周期性。Stats 抽头必须在 **LSC 之后、重 tonemap 之前的线性域**（与 AE 同一原则），否则条纹被 gamma 压弯、被美颜抹掉。

### 1.1 逐行行积分差分法（Row-Sum Difference / Projection）

**原理**

计算每行（或 Row-block 条带）平均亮度：

$$
S_t(y) = \sum_x I_t(x, y)
$$

对相邻两帧做差分或归一化比值，消掉场景静态纹理，只留随时间相位移动的条纹：

$$
\Delta S(y) = \bigl|S_t(y) - S_{t-1}(y)\bigr)
\quad\text{或}\quad
R(y) = \frac{S_t(y)}{S_{t-1}(y)}
$$

单帧 \(S_t(y)\) 里纹理和条纹混在一起；**帧差把“钉在场景上的结构”消掉，条纹因相位在两帧间滑动而留下**。

**判定**

1D 信号在垂直方向应有空间周期 \(\lambda\)。用

$$
\lambda = \frac{T_{\mathrm{flicker}}}{T_{\mathrm{line}}}
$$

把实测周期与 100 Hz / 120 Hz 理论条纹间距比匹配度（允许 ±1～2 bin 的量化误差）。

### 1.2 频域分析法（FFT / DFT / Autocorrelation）

对消除纹理后的 1D 行亮度差分做一维 FFT 或快速自相关。

**判定**

- 在 100 Hz、120 Hz 对应的**空间频率**点 \(f_{100}\)、\(f_{120}\) 看能量谱峰值。
- 用 SNR 或 Peak-to-Average Ratio 过阈值，区分 50 Hz vs 60 Hz vs 无频闪。

空间频率（cycles / frame-height）与时间频率的关系：

$$
f_{\mathrm{spatial}} = f_{\mathrm{flicker}} \cdot T_{\mathrm{line}} \cdot N_{\mathrm{rows}}
\quad\text{（一整帧读出时间内经过多少个闪烁周期）}
$$

ISP 里更常见的是 **Goertzel**（只算两个已知频点）而不是整段 FFT。

### 1.3 差分统计与极值计数（Zero-Crossing / Peak Detection）

轻量 DSP / ISP 模块上避免 FFT：对行均值差分做 LPF，直接数波峰 / 波谷、量过零点间距。

**特点：** 乘加极少，抗高频噪点靠前级滤波；复杂运动（行人、手持晃动）容易把运动边缘当成条纹 → **False Positive**。工业实现几乎一定叠加平坦度掩膜 + 多帧滞后。

---

## 2. 基于专用 Flicker 传感器（Hardware Flicker Sensor）

RS 图像算法受曝光时间、帧率、场景运动干扰，且 **检测上限被行读出速度卡住**（工频 100/120 Hz 还行，数 kHz 的 LED PWM 看不清）。高端手机与专业相机普遍加多通道环境光 / 频闪芯片（AMS-OSRAM、ST 等）。

### 2.1 时域高速采样 + 频域分析（Direct Temporal FFT）

专用传感器以 1 kHz～10 kHz+ 采样环境光波形 \(x(t)\)，与相机曝光、图像内容完全解耦。

**算法**

1. DC 滤除 + Hanning / Hamming 窗。
2. 实时 FFT，定位基频（100 / 120 / 300 Hz，以及 1～5 kHz PWM）。
3. 预览开流之前就能锁频率。

**优势：** 免疫运动与复杂纹理；暗光 / 高光动态范围通常优于从已量化的 Bayer 里抠条纹。

### 2.2 高频 PWM 与 LED Flicker Mitigation (LFM)

车载后视、拍 LED 屏 / 车灯时，闪烁远高于工频。专用传感器给出 PWM 频率与占空比；ISP / sensor 侧用：

- 双曝光融合，或
- 像素级分离曝光（Split-diode / LOFIC）

在硬件层压高频伪影，而不是只靠 AE 把 \(t_{\mathrm{exp}}\) 锁到 10 ms 网格（那对 kHz PWM 无效）。

---

## 3. 算法对比与工程挑战

| 维度 | 软件行统计差分 / FFT | 专用硬件 Flicker 传感器 |
|------|----------------------|-------------------------|
| 硬件成本 | 零附加 BOM | 占板级空间与成本 |
| 检测上限 | 受 \(T_{\mathrm{line}}\) 限制，通常 ≤ 120 Hz 工频 | 可到数 kHz LED PWM |
| 运动鲁棒性 | 物体运动 / 手持晃动易误检 | 与图像运动无关 |
| 极暗 / 极亮 | 依赖当前曝光；暗光噪点淹没条纹 | 大动态，低照度仍能抓波动 |
| 与 AE 耦合 | 曝光已是整数倍时条纹消失，检测会“看不见”→ 需要保持/记忆态 | 不依赖当前 \(t_{\mathrm{exp}}\) |

工程上常见组合：**专用传感器做主判决，ISP 行统计做 sanity / 无传感器 SKU 的回退**。

---

## 4. 工业级抑制：3A 闭环联动

检测到频率后，AE 进入防频闪状态机（详见 [3A.md](./3A.md) 的 `snap_to_antibanding`）：

**AE 步长量化（Step Quantization）**

强制曝光时间落在半周期整数倍：

- 50 Hz：\(1/100,\; 2/100,\; 3/100,\; \ldots\) 即 10 ms、20 ms、30 ms、…
- 60 Hz：\(1/120,\; 2/120,\; \ldots\) 即 8.33 ms、16.67 ms、…

增益补足 `Pnext / t` 的剩余部分。帧率不够长时改 VTS / 降 fps，而不是偷偷用非网格曝光。

**滞后与时域平滑（Hysteresis & Temporal Smoothing）**

混合光源（窗边日光 + 室内灯）、弱频闪时，50/60 会来回跳。需要：

- 进入阈值 > 退出阈值；
- 连续 \(N\) 帧同判决才切换；
- 已锁定后，即使当前曝光已消条纹（检测 SNR 掉下去）也保持 mode，直到明确看到另一种频率或长时间无峰值。

**分块置信度加权（Spatial Confidence Weighting）**

按块计算平坦度（方差 / 梯度能量）。只在低纹理区域（天花板、纯色墙）提频闪特征，丢掉高频纹理和运动边缘。这是软件法把 False Positive 压下来的主手段。

---

## 5. ISP 内：行差分法的定点化流程

本节回答「怎么在 ISP / DSP 上、不用浮点、按行 stats 做完检测」。目标：每帧在 V-blank 内出判决，bit-exact 可回归。

### 5.1 数据从哪来（不要吃全图）

```text
Bayer 线性域（BLC → LSC 之后）
        │
        ▼
IFE Stats Engine  ──  硬件累加 Row-Sum / Row-block
        │                 每行或每 k 行一个 uint32
        ▼
SRAM / 3A 统计包（高度方向 N 点，N = H 或 H/k）
        ▼
DSP 定点 flicker 模块  ──  差分 → LPF → 周期/Goertzel → 状态机
        ▼
AE antibanding mode ∈ {OFF, 50Hz, 60Hz, HOLD}
```

- **不要**对全分辨率 Bayer 做行求和：4K 宽 × 12 bit 的软件累加会打满带宽；这是 Stats HW 的工作。
- Row-block（例如每 4～8 行合一个 bin）降低 \(N\)，也等效低通，有利于压读出噪声。
- 只用 **G 通道或 Y≈(2G+R+B)/4** 的行和，色通道 SNR 更差、条纹主要是亮度。
- 与 AE 共用同一份线性 stats；切勿用 gamma 后的 YUV preview。

### 5.2 定点预算（先算位宽再写代码）

设行宽 \(W=4096\)，像素 12 bit 线性、已减黑电平。一行求和：

$$
S_{\max} = 4096 \times (2^{12}-1) \approx 2^{24}
$$

放进 `uint32_t` 有余量。后面所有「均值、比值、滤波」都用 **Q 格式 + 先加宽再圆整**，规则与 [fixed_point.md](../XR/Meta_Camera/code/fixed_point.md) 相同：乘法走 `int64_t`，正数 `+half`、负数 `-half`。

| 量 | 建议格式 | 理由 |
|----|----------|------|
| 行和 \(S(y)\) | `uint32` | 硬件累加原样 |
| 行均值 \(\mu(y)\) | Q16.16 `int32` | \(S \times \mathrm{inv}W\)，`invW = round(2^{16}/W)` 预存在配置 |
| 帧差 \(d(y)\) | Q16.16 `int32` | 可正可负，**必须有符号** |
| 归一化残差 \(r(y)\) | Q3.12 或 Q8.8 | 相对变化通常 ≪ 1；满幅 8 足够 |
| IIR \(\alpha\) | Q1.15 | \(\alpha \in (0,1)\) |
| Goertzel 系数 | Q1.15 / Q2.14 | \(\cos\omega\) 落在 \([-1,1]\) |
| 功率谱 \(P\) | `uint32` 能量（平方后右移） | 只比大小，不必还原成物理单位 |

**禁止：** `(int32)a * b >> 16` 不先扩到 64 bit。行均值一旦 > 1.0（Q16.16 里很常见），静默溢出，频谱全是垃圾。

除法一律换成「乘倒数 + 移位」。`1/W`、`1/μ_{t-1}` 的倒数在本帧开头算一次（牛顿迭代 3～4 次，或配置期算好 `invW`）。

### 5.3 流水线各级

```text
S_t[y]          硬件 Row-Sum
   │
   ▼
μ_t[y] = (S_t[y] * invW + half) >> Q     行均值
   │
   ▼
纹理消除（二选一，工业常用比值）
   d[y] = μ_t[y] - μ_{t-1}[y]            绝对差：运动时残留大
   r[y] = μ_t[y] * inv(μ_{t-1}[y]) - 1   相对差：曝光微变时更稳
   │
   ▼
空间置信度掩膜
   若 block_var[y] > T_texture 或 motion_flag[y] → r[y]=0
   │
   ▼
LPF（沿 y 的 1D IIR 或 5-tap FIR）
   y[n] = α r[n] + (1-α) y[n-1]          定点 MAC
   │
   ▼
特征提取（三选一，按算力）
   A. 过零间距直方图 → 众数 vs λ_100 / λ_120
   B. 自相关 R(τ) 在 τ=λ_100、λ_120 取峰
   C. Goertzel 只算两个频点的功率 P100、P120
   │
   ▼
SNR = P_peak / P_avg，过阈值 + 滞后状态机
   │
   ▼
mode → AE snap_to_antibanding
```

#### 纹理消除：为什么用比值

绝对差分 \(\Delta S\) 在亮区和暗区尺度不同，运动物体是大幅度残差。相对量

$$
r(y) = \frac{\mu_t(y)}{\mu_{t-1}(y)} - 1
$$

把条纹变成「百分之几的行间波动」，阈值可做成与 Lux 无关。定点实现：

```c
// μ 为 Q16.16；inv_mu 用牛顿法求 1/μ_{t-1}，Q16.16
int32_t ratio_q16 = qmul_q16(mu_t[y], inv_mu_tm1[y]);  // ≈ 1.0 + r
int32_t r_q16     = ratio_q16 - (1 << 16);
```

\(\mu_{t-1}\approx 0\)（死黑行、OP 裁切）要跳过，否则倒数爆炸。

#### 平坦度掩膜

对每个 Row-block 用 AE 已有的 BG 块方差，或对本行 \(S\) 的水平方向不再统计——ISP 通常已有 2D Bayer Grid。规则：

- `flatness = 1`（方差低于门限）才累进 Goertzel / 过零；
- 人脸 / 运动块直接置零。

没有掩膜，窗帘竖条、百叶窗会稳定地“检出”一个假周期。

#### 低通

5-tap 对称 FIR 系数可用整数 `[1, 4, 6, 4, 1]`（\(\times 16\) 归一，全是移位）：

```c
acc = r[i-2] + (r[i-1] << 2) + r[i]*6 + (r[i+1] << 2) + r[i+2];
lp[i] = (acc + 8) >> 4;   // +8 圆整
```

IIR 更省存储（只要一个 state），\(\alpha\) 取 Q1.15 的 \(1/8\) 或 \(1/16\)，暗光多滤一点。

### 5.4 两种判决核的定点写法

#### A. 过零 / 峰间距（最便宜）

对 `lp[y]` 找符号变化，记录相邻过零的 \(\Delta y\)。理论间距：

$$
\lambda_{100} = \mathrm{round}\bigl(T_{50} / T_{\mathrm{line}}\bigr),\quad
\lambda_{120} = \mathrm{round}\bigl(T_{60} / T_{\mathrm{line}}\bigr)
$$

\(T_{\mathrm{line}}\) 从当前 sensor mode 的 line length / MIPI 时钟算出来，**随 fps、binning 变**，不能写死 1000 行。把所有 \(\Delta y\) 投到直方图，在 \(\lambda_{100}\pm\mathrm{tol}\) 与 \(\lambda_{120}\pm\mathrm{tol}\) 两个窗口内积分，谁大且过最小事件数谁赢。

运动误检：过零又密又乱，直方图没有孤立峰 → 输出 UNCERTAIN，保持上一 mode。

#### B. Goertzel（推荐的 ISP 折中）

只计算 \(z = e^{\pm j\omega}\) 上的 DFT 一点，\(\omega = 2\pi / \lambda\)。递推：

$$
s[n] = x[n] + 2\cos\omega \cdot s[n-1] - s[n-2]
$$

功率：

$$
P = s_N^2 + s_{N-1}^2 - 2\cos\omega \cdot s_N s_{N-1}
$$

定点注意：

1. `coeff = 2*cosω` 用 Q2.14，范围 \((-2,2)\)，乘加必须 `int64`。
2. \(s[n]\) 会按 \(N\) 放大，中间变量用 `int64`，每若干点右移保位（block floating point），最后比较 \(P_{100}\) 与 \(P_{120}\) 时两边要同一缩放。
3. 同时跑 \(\omega_{100}\) 和 \(\omega_{120}\) 两套 state，再加一个「旁瓣」频点当噪声底 \(P_{\mathrm{noise}}\)。

$$
\mathrm{SNR}_{100} = \frac{P_{100}}{P_{\mathrm{noise}}},\quad
\mathrm{conf} = \frac{|P_{100}-P_{120}|}{P_{100}+P_{120}}
$$

50/60 接近时（行数少、一帧里周期数不够）`conf` 低，禁止切换。

整段 FFT 只在 PC 仿真 / 标定工具里跑，用来签 Goertzel 的 bit-exact 黄金模型。

### 5.5 状态机（检测与 AE 解耦）

```text
        SNR < T_enter
     ┌──────────────────┐
     │      OFF         │
     └────────┬─────────┘
              │ SNR50>T_enter 且 conf 高，连续 N 帧
              ▼
         LOCK_50  ←──────────  对称地 LOCK_60
              │
              │ 另一种频率 SNR 更高且连续 M 帧（M>N）
              │ 或 SNR 长期 < T_exit → HOLD → OFF
              ▼
            HOLD   （曝光已对齐，条纹消失，检测变瞎）
```

**HOLD 是软件法的关键态：** 一旦 AE 把 \(t_{\mathrm{exp}}\) 锁到 10 ms，行间条纹被物理消掉，差分信号 SNR 会掉。若没有 HOLD，下一帧会判 OFF，曝光离开网格，条纹回来，再锁上……预览闪烁。HOLD 靠专用传感器、或靠「上次高置信锁定后保持直到明确反证」。

输出给 AE 的不是瞬时 FFT 峰值，而是这个状态机的 mode。

### 5.6 伪代码（DSP 每帧）

```c
// 输入: row_sum_t[N] uint32, 上一帧 mu_tm1[N] Q16.16
// 配置: invW_q16, T_line_ns, alpha_q15, 掩膜 flat[N]

for (int y = 0; y < N; ++y) {
    mu_t[y] = (int32_t)(((uint64_t)row_sum_t[y] * invW_q16 + (1u << 15)) >> 16);
}

int64_t p100 = 0, p120 = 0, pnoi = 0;
int32_t lp = 0;
for (int y = 0; y < N; ++y) {
    if (!flat[y] || mu_tm1[y] < MU_MIN) { lp = qmul(alpha, lp); continue; }
    int32_t inv = qrecip_q16(mu_tm1[y]);          // 牛顿倒数
    int32_t r   = qmul_q16(mu_t[y], inv) - Q16_ONE;
    lp = qmul_q15(alpha_q15, r) + qmul_q15(Q15_ONE - alpha_q15, lp);
    goertzel_step(&g100, lp);
    goertzel_step(&g120, lp);
    goertzel_step(&gnoi, lp);
}
p100 = goertzel_power(&g100);
p120 = goertzel_power(&g120);
pnoi = goertzel_power(&gnoi) + EPS;

mode = flicker_hysteresis(p100, p120, pnoi, prev_mode);
memcpy(mu_tm1, mu_t, N * sizeof(int32_t));
```

### 5.7 验证与会翻车的点

| 坑 | 现象 | 对策 |
|----|------|------|
| 曝光已在网格上 | 条纹消失，检测报 OFF，随后 banding 回来 | HOLD 态；或独立 flicker sensor |
| \(T_{\mathrm{line}}\) 随 mode 变 | 切 60 fps / binning 后 50/60  ident 反了 | 每个 sensor mode 重算 \(\lambda\)、\(\omega\) |
| 未加宽乘法 | 亮场频谱乱、暗场偶尔对 | 全程 `int64` MAC |
| 无平坦度掩膜 | 百叶窗、斑马线稳定假 50 Hz | 用 BG 方差 / 梯度剔块 |
| 单帧判决 | 手持晃一下就切 50↔60 | \(N\) 帧确认 + 进入/退出双门限 |
| Stats 抽在 gamma 后 | 暗条被抬亮，周期畸变 | 与 AE 同抽头：LSC 后线性域 |
| 全局 shutter / 极短曝光 | 几乎无空间条纹 | 软件法失效，靠专用传感器或 PWM LFM |
| 高频 LED PWM | 行统计看不到 kHz | 不要假装能检；报 UNKNOWN，走 LFM / 传感器 |

实验室签核：可调频灯箱扫 50 / 60 / 混合 / PWM 1～5 kHz，加运动滑轨与高纹理图表，看 False Positive / 锁定时间 / 切 mode 是否引起亮度跳变（应被滞后吃掉）。

---

## 6. 面试怎么收口

**一句话：** 频闪检测是测 \(T_{\mathrm{flicker}}\)，anti-banding 是把曝光锁到 \(k\cdot T_{\mathrm{flicker}}\)；软件法吃的是 RS 行统计的 1D 周期，定点化就是行和 → Q 均值 → 帧间相对差 → 掩膜 + LPF → Goertzel/过零 → 带 HOLD 的滞后状态机。

**和 3A 的边界：** 检测模块只出 mode 与 confidence；AE 负责 `snap_to_antibanding` 和增益补齐。不要在检测里改曝光。

**眼镜 / 车载追问：** tracking 相机常固定曝光，软件条纹检测更瞎；工频灯下应用 GS + 曝光网格，PWM 灯用专用传感器或 LFM。媒体预览路径才跑完整 anti-banding 状态机。
