# 高通 Camera Sensor / CSIPHY / CSID

> V4L2 / DMA / UBWC 见 [camera_driver.md](./camera_driver.md)。

## 一、CSIPHY vs CSID

在 Camera 系统的硬件接收端（SOC 侧），**CSIPHY** 和 **CSID** 分别处于 **OSI 模型的物理层（Physical Layer）** 与 **数据链路层（Data Link Layer）**，它们是上下游的协作关系。

一句话区别：**CSIPHY 负责「把电信号变成 0 和 1」；CSID 负责「把 0 和 1 组装成有意义的图像包」。**

### 1. 核心区别对比

| 维度 | CSIPHY (CSI Physical Layer) | CSID (CSI Decoder / Receiver) |
| --- | --- | --- |
| **层级定位** | **物理层 (PHY Layer)** | **协议/数据链路层 (Protocol / Data Link Layer)** |
| **处理对象** | 模拟电信号（微伏级别的差分电压/相位变化） | 数字比特流（RAW Data、Header、Footer） |
| **核心职责** | 信号解调、时钟恢复、高速串并转换（Deserialization） | 解包（Unpack）、虚拟通道路由、数据类型识别、ECC/CRC 校验 |
| **硬件位置** | 紧挨着芯片物理 PAD（接脚）的高速模拟/混合信号电路 | SOC 内部的纯数字逻辑电路 |
| **配置重点** | MIPI 模式（D-PHY/C-PHY）、Settle Time、Lane 数量 | Virtual Channel (VC)、Data Type (DT)、Image Format |

### 2. 工作流程与职责详解

当 Sensor 将采集到的像素通过 MIPI 铜线发给高通 SOC 时，数据先经过 **CSIPHY**，再送到 **CSID**：

```text
 [ Sensor 芯片 ]
        │
        │ (高频模拟电信号: 差分电压或相位)
        ▼
 [ CSIPHY 模块 ]  ──────► 1. 锁相环（PLL）锁定信号与时钟
        │                2. 将模拟信号解调为高频串行 0101...
        │                3. 串并转换（Deserialization），输出为并行 Byte 数据
        │
        │ (并行数字字节流 + Byte Clock)
        ▼
 [ CSID 模块 ]    ──────► 1. 识别包头 (SoF, EoF, Packet Header)
        │                2. 检查 CRC/ECC 校验（丢弃损坏包）
        │                3. 根据 VC (Virtual Channel) 剥离数据，识别 DT (如 RAW10/RAW12)
        │                4. 将 Raw 数据转为宽位宽（如 64/128-bit），推给 IFE/ISP
        ▼
 [ IFE / ISP (Image Processing) ]
```

#### CSIPHY：专注于「电气与信号层」

CSIPHY 是离 Sensor 物理引脚最近的电路。它的主要挑战是**应对极高频率下的信号衰减和噪声**。

- **物理模式兼容**：支持 **D-PHY**（按差分电压高低电平判定 0/1）或 **C-PHY**（按三线间的相位/电压差变转换判定 0/1）。
- **Clock 恢复与 Settle Time**：Sensor 刚发送信号时存在电平过渡期的抖动（HS-Settle）。CSIPHY 驱动必须配置正确的 `settle_time` 寄存器，让 PHY 在信号稳定后才开始采样，否则会导致 **MIPI Clock 不 Lock 或帧同步丢失**。
- **串并转换**：把吉比特每秒（Gbps）级别的单线串行数据，转成几十 MHz 频率的 8-bit 或 16-bit 并行字节（Byte）流。

#### CSID：专注于「协议与数据结构」

CSID 拿到的已经是干净的「数字字节流」了，它不再关心电压是多少，而是**根据 MIPI CSI-2 协议标准去解析数据包**。

- **包解析与对齐**：识别帧头（SoF - Start of Frame）、帧尾（EoF）、行头（SoL）和行尾（EoL）。
- **多路复用路由（VC & DT 解析）**：
  - **VC (Virtual Channel)**：如果多个 Sensor 共享同一组 MIPI 物理 Lane，或者一个 Sensor 输出多路 Stream（如 RGB + Depth），CSID 根据 VC ID（VC0~VC3）把数据流分发给不同的 ISP 处理通道。
  - **DT (Data Type)**：识别当前包是 RAW8 (`0x2A`)、RAW10 (`0x2B`)、RAW12 (`0x2C`) 还是 3A Stats/Embedded Data。
- **数据解包与对齐（Unpack）**：如 MIPI RAW10 在传输时为了省带宽，5 个字节存 4 个像素（Packing 模式）。CSID 会将其解包（Unpack）为标准 10-bit 或 16-bit 对齐的像素，方便后端的 IFE/ISP 进行矩阵计算。

### 3. 驱动 Bring-up 时的故障排查区分（AE 实战）

在 Sensor 点亮调试时，区分 CSIPHY 和 CSID 能帮你快速定位硬件/软件问题：

- **如果问题在 CSIPHY 层**：
  - **现象**：`CSIPHY clk non-lock`、`FIFO overflow`、全屏严重花屏或彻底拿不到任何数据。
  - **排查方向**：检查 MIPI 物理硬件走线、测量 Lane 电压、检查 Sensor MCLK/PCLK 时钟、调整驱动中的 `T_HS_SETTLE` 参数。
- **如果问题在 CSID 层**：
  - **现象**：CSIPHY 已 Lock，但 Log 报 `CSID CRC error`、`CSID ECC error`、`UnSupported Data Type` 或 `Frame start without Frame end`。
  - **排查方向**：检查 Sensor 寄存器配置的 **Data Type / Virtual Channel** 与高通 CSID 驱动里的配置是否一致；检查 Sensor 输出的分辨率（HTS/VTS）是否与 CSID 设定的 Crop 尺寸冲突。

---

## 二、Sensor 配置：CCI / MIPI / 寄存器 / 曝光增益

在移动端 Camera 系统中，Sensor 驱动是软硬件交互的第一站。理解 Sensor 配置，本质上就是弄清楚：

1. **控制信号怎么发过去（CCI/I2C）**
2. **图像数据怎么传回来（MIPI CSI-2）**
3. **关键参数怎么实时控制（曝光/增益/寄存器）**

### 1. 控制通道：CCI (Camera Control Interface) 与 I2C

Sensor 是一个通过 **I2C 协议** 读写寄存器的外设。但在高通平台，驱动并不是直接调用 Linux 通用的 `i2c-core` 驱动，而是使用高通专用的 **CCI 硬件模块**。

- **传统的 I2C 通信**：由 CPU 触发并轮询控制，传输速度慢（标准模式 100kbps，快速模式 400kbps）。在相机每帧（Per-frame）都需要配置数十个寄存器时，CPU 开销过大，容易造成延迟。
- **高通 CCI (Camera Control Interface)**：
  - **本质**：高通 SOC 内置的一个专门用于控制 Camera 外设的 **硬件 Master 控制器**（支持 I2C/I3C 协议）。
  - **硬件队列与 DMA**：CCI 拥有自己的 FIFO 队列和 Command Engine。内核驱动（CamX KMD）只需要将一整包寄存器地址和数值写入 CCI 队列，CCI 硬件就会在后台通过 DMA 自动将数据发给 Sensor，**完全解放 CPU**。
  - **Master 映射**：高通 SOC 通常有多个 CCI Master（如 CCI Master 0/1），分别连接主摄、副摄等不同的 Sensor，支持多路并行控制。

### 2. 数据通道：MIPI CSI-2 接收与物理层

Sensor 采集到光信号转为数字信号（Bayer RAW）后，通过 **MIPI CSI-2** 高速总线传输给高通 SOC 的 **CSIPHY/CSID** 模块。

```text
 [ Sensor 芯片 ]  ──( differential pairs )──► [ CSIPHY (D-PHY/C-PHY) ] ──► [ CSID (Demux) ] ──► [ IFE / ISP ]
```

1. **PHY 物理层（D-PHY vs C-PHY）**：
   - **D-PHY**：使用 1 对差分 Clock Lane + 1~4 对差分 Data Lane。数据在 Clock 的上下沿传输（DDR）。
   - **C-PHY**：取消了独立的 Clock Lane，采用 3 线一组（Trio）的相位调制编码，嵌入时钟信号，在有限带宽下提供更高的传输速率（比 D-PHY 节省线数且功耗更低）。
2. **CSID (CSI Decoder)**：
   - 负责解析 MIPI 包头，识别 **Virtual Channel (VC)** 和 **Data Type (DT)**（例如 DT=`0x2B` 代表 Raw10，DT=`0x2C` 代表 Raw12）。
   - 将 MIPI 串行流解包（Unpack）为并行数据，送入 IFE (Image Front End) 进行 ISP 图像处理。

### 3. Sensor 寄存器配置与 Mode 切换

Sensor 驱动的核心工作之一，就是维护一组结构化的**寄存器配置表（Register Arrays）**。

#### Init Settings 与 Mode Settings

- **Init Settings（点亮配置）**：Sensor 上电（Power-up）后写入的第一组寄存器。用于配置 Sensor 内部模拟电路、PLL 锁相环（时钟倍频）、ADC 精度、温度传感器等底层硬件。
- **Mode Settings（模式配置）**：根据 App 请求的分辨率与帧率（如 1080p@60fps 预览 vs 50MP 拍照 vs 4K@120fps 慢动作），切换 Sensor 的工作模式。主要涉及：
  - **Resolution / Cropping**：输出的 Active 像素区域。
  - **Binning / Skipping**：像素合并（如 4-in-1 邻近像素相加以提高感光度）或跳行读取。
  - **Pixel Clock (VT_PIX_CLK) & Frame Timing**：配置 `line_length_pclk`（行长）与 `frame_length_lines`（帧长）。

#### 时序与帧率计算公式（重要面试常考点）

Sensor 的输出帧率是由其内部的**虚拟像素时钟（Pixel Clock）**和**帧几何尺寸**严格决定的：

$$
\mathrm{FPS} = \frac{\mathrm{PixelClock}}{\mathrm{HTS} \times \mathrm{VTS}}
$$

其中 `PixelClock` 单位为 Hz；`HTS` = Line Length Pclk；`VTS` = Frame Length Lines。

- **HTS (Horizontal Total Size / Line Length)**：每一行所包含的有效像素 + 灭隐区（H-Blank）的总 clock 数。
- **VTS (Vertical Total Size / Frame Length)**：每一帧所包含的有效行数 + 灭隐区（V-Blank）的总行数。
- **如何修改帧率**：当需要降低帧率（例如暗光下从 30fps 降到 15fps 以延长曝光时间）时，驱动通常会保持 HTS 不变，通过动态拉长 VTS（增加 V-Blank 行数）来实现。

### 4. 实时控制：Exposure（曝光）与 Gain（增益）

在相机运行过程中，3A 算法（AE Engine）会根据每一帧的画面亮度，实时计算出下一帧需要的**曝光时间（Integration Time）**和**增益（Gain）**，并交由驱动写入 Sensor。

#### 曝光控制（Exposure / Integration Time）

- **控制原理**：Sensor 的曝光是通过控制电子快门的重置（Reset）与读取（Readout）之间的时间差来实现的。
- **寄存器单位**：Sensor 的曝光时间通常**以「行（Lines）」为单位**，而不是直接写微秒（µs）。

$$
\mathrm{ExposureTime} = \mathrm{CoarseIntegrationLines} \times \frac{\mathrm{HTS}}{\mathrm{PixelClock}}
$$

单位：`ExposureTime` 为秒；`CoarseIntegrationLines` 为曝光行数。

- **VTS 限制**：**曝光行数不能超过当前模式的 VTS（Frame Length）**。如果 AE 算出的曝光行数大于当前 VTS，驱动必须**同步将 VTS 拉长**，否则曝光将无法生效或导致图像错帧。

#### 增益控制（Analog & Digital Gain）

当曝光时间达到上限（如帧率限制）时，就需要通过增加 Gain 来提亮画面：

- **Analog Gain (AG / 模拟增益)**：在 ADC 采样前对像素传感器的模拟电荷信号进行放大。**噪点增加少，画质好，应优先使用**。
- **Digital Gain (DG / 数字增益)**：在 ADC 转为数字信号后对数值进行乘算放大。**会同步放大噪声，容易导致画质劣化**，通常在 Analog Gain 达到硬件上限后才补充使用。
- **Gain Code 转换**：Sensor 芯片通常不直接接收分贝（dB）或倍数（1x, 2x），驱动内部需要将算法给出的 ISO/Gain 倍数，根据 Sensor 厂商（如 Sony/Samsung/OmniVision）的特定公式，转换为寄存器需要填入的 **Gain Code**。

### 5. 全链路总结（时序与数据流）

```text
[ App / 3A 算法 ] ──计算出 Exposure / Gain ──► [ CamX KMD ]
                                                     │
                                                     ▼ (构建 CCI Command)
                                            [ Qualcomm CCI 硬件 ]
                                                     │
                                                     ▼ (I2C 协议)
                                            [ Sensor 寄存器 ]
                                                     │
                             (在 V-Blank 期间更新曝光并开始下一帧曝光)
                                                     │
                                                     ▼ (Bayer RAW 数据)
                                         [ MIPI CSI-2 (D/C-PHY) ]
                                                     │
                                                     ▼
                                            [ SOC CSIPHY / CSID ] ──► [ ISP / IFE ]
```

