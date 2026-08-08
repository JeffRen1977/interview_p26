# 高通 Camera 驱动面试：V4L2 / DMA / UBWC

> Sensor / CSIPHY / CSID 见 [sensor.md](./sensor.md)。

V4L2（Video for Linux 2）是 Linux 内核级的通用多媒体/视频子系统框架，它不仅控制 ISP，还负责整条 Camera 硬件链路的配置与数据流管理。

在高通平台和 Linux 内核中，V4L2 是连接 **Linux Kernel 驱动** 与 **Android Camera HAL / CamX** 的桥梁。

---

## 一、V4L2 到底控制什么？（全链路视角）

V4L2 采取的是 **Sub-device（子设备）** 架构。整条 Camera 硬件 pipeline 上的各个组件，在 Linux 内核中都被抽象为独立的 V4L2 子设备（`v4l2_subdev`）：

```text
[ Sensor ] ──(MIPI CSI-2)──► [ CSIPHY / CSID ] ──► [ ISP (IFE/IPE) ] ──► [ System Memory (dma-buf) ]
    │                              │                      │                        │
  v4l2_subdev                    v4l2_subdev            v4l2_subdev             /dev/video*
(Exposure/Gain)                (Lane config)          (Format/Crop)            (Buffer Stream)
```

1. **Sensor 控制**：曝光时间（Exposure）、增益（Gain）、帧率（FPS）、Test Pattern、Mode 切换（通过 `v4l2_control` 或 `v4l2_subdev_core_ops`）。
2. **MIPI CSI 接收端**：MIPI Lane 数量配置、Clock 频率、Data Format 对齐。
3. **ISP 控制**：
   - **Format / Stream Config**：配置 Input/Output 格式（Raw10, Raw12, NV12 等）、Crop（裁剪）、Scaling（缩放）。
   - **ISP 硬件流水线启动/停止**：`VIDIOC_STREAMON` / `VIDIOC_STREAMOFF`。
   - **Stats 数据提取**：3A 统计数据（AF, AE, AWB Stats）从 ISP 节点向用户态（CamX）的传递。
4. **Buffer 内存流控（最关键）**：基于 `videobuf2` (vb2) 机制管理 `dma-buf`，处理 `VIDIOC_REQBUFS`、`VIDIOC_QBUF`（入队）、`VIDIOC_DQBUF`（出队）。

---

## 二、在高通 CamX / Chi 架构中，V4L2 扮演什么角色？

在高通平台，用户态的 **CamX 并不是直接操作裸硬件**，而是通过 Linux 的 V4L2 接口与 Kernel 里的高通 ISP 驱动（如 `msm_cam` / `cam_isp`）进行交互：

- **控制流（Control Path）**：CamX 里的 `Node`（如 `IFE Node`）通过 IOCTL 调用 V4L2 接口配置 ISP 硬件寄存器、设置 Stream 格式。
- **数据流（Data Path）**：CamX 将 Android Gralloc 分配的 `dma-buf` 内存句柄通过 V4L2 的 `VIDIOC_QBUF` 送给内核 ISP 驱动，ISP 硬件通过 DMA 直接写入该内存，写完后通过中断触发 `VIDIOC_DQBUF` 通知 CamX 取走数据。

---

## 三、面试提问切入点（Senior Staff 级别）

如果面试官在 V4L2 环节深入追问，通常会考察以下硬核细节：

- **Media Controller 框架**：V4L2 如何利用 Media Controller (`/dev/media*`) 将 Sensor、CSID、IFE、IPE 抽象成 Node 与 Link 并建立 Pipeline Graph（拓扑图）？
- **V4L2 Buffer 零拷贝（Zero-Copy）**：`V4L2_MEMORY_DMABUF` 机制如何避免 CPU 拷贝，直接让 ISP 硬件 DMA 写入 Application Memory？
- **异步驱动加载与 Bind**：V4L2 异步子设备注册（`v4l2_async_register_subdev`），Sensor 与 ISP 驱动是如何在 Kernel 启动时完成匹配与 Bind 的？

在高通 Camera 驱动体系中，**V4L2（Video for Linux 2）** 并不是直接去操作 ISP 硬件寄存器，而是作为一层标准的**内核字符设备接口（Linux V4L2 Framework）**，向下包装高通自定义的 **CamX KMD（Kernel Mode Driver）/ KMD Sub-devices** 驱动架构。

掌握高通 V4L2 的底层实现原理，需要重点理解三个维度的实现：**控制流交互机制（参数配置）**、**数据流与内存管理（DMA & Memory）** 以及 **内核/硬件层的响应**。

---

## 四、高通 V4L2 控制流：参数配置的底层实现原理

用户态（CamX/Chi-HAL）配置 Camera 参数（如 Exposure、Gain、Format、Resolution、Crop）并不是简单调用传统的 `v4l2_control`，而是通过高通精心设计的 **`VIDIOC_MSM_CAM_CONTROL`（或标准的 `ioctl` 扩展）** 将批量配置打包下发。

### 1. 批量指令下发机制（Payload & Config Packets）

由于相机每帧（Per-frame）需要配置的参数极多（3A 结果、ISP 各 Node 寄存器、Sensor 曝光/增益等），为了避免频繁地从用户态切换到内核态，高通采用了批量数据包（Config Packet）机制：

```text
[ CamX (User Space) ] ── (打包结构体/Cmd Buffer) ──► ioctl(VIDIOC_MSM_CAM_CONTROL)
                                                            │
                                                            ▼
                                                [ V4L2 / Subdev Driver ]
                                                            │
                                                            ▼
                                              [ Parse Header & Subdev Ops ]
                                                            │
                                      ┌─────────────────────┼─────────────────────┐
                                      ▼                     ▼                     ▼
                               [ Sensor Subdev ]     [ IFE Subdev ]        [ IPE Subdev ]
                               (I2C/CCI Direct)      (Direct MMIO)         (Direct MMIO)
```

1. **CamX 打包**：CamX 建立一个 Command Buffer，将要修改的 Sensor 寄存器列表、ISP 权重矩阵等序列化为一串内存 Packet。
2. **`ioctl` 传输**：通过 V4L2 子设备的 `ioctl`（如 `/dev/v4l-subdev*`）将物理地址/虚拟地址指针传给 KMD。
3. **KMD 解析与分发**：
   - **Sensor 参数**：KMD 解析 Command Packet 后，通过 **CCI (Camera Control Interface) / I2C** 总线以 DMA 或 FIFO 方式批量写给 Sensor。
   - **ISP 参数**：KMD 直接通过 **MMIO（内存映射 I/O，`iowrite32`）** 批量写入 IFE/IPE 的硬件寄存器，或者将配置数据写入硬件的 **CDM (Camera Data Mover)** 引擎，由 CDM 自动搬运至 ISP 硬件寄存器。

---

## 五、内存管理与大小计算：DMA & Zero-Copy 底层原理

Camera 数据流的吞吐量极大（如 4K 60fps 原始数据吞吐可达几 Gbps），因此高通 V4L2 绝对避免 CPU 参与内存拷贝，完全基于 **`videobuf2` (vb2) + `dma-buf` (ION)** 实现硬件零拷贝（Zero-Copy）。

### 1. 内存配置与分配全流程

1. **用户态分配（Gralloc / ION）**：
   - 内存由用户态的 Gralloc 或 ION/dma-buf 分配器在 SMMU（System MMU）可寻址的连续物理/虚拟区域申请。
   - **关键数据结构**：获取到一个文件描述符 `fd`（`dma_buf_fd`）。
2. **通过 V4L2 告知内核（`VIDIOC_REQBUFS` & `VIDIOC_QBUF`）**：
   - CamX 调用 `ioctl(VIDIOC_REQBUFS)`，设置模式为 `V4L2_MEMORY_DMABUF`。
   - CamX 调用 `ioctl(VIDIOC_QBUF)`，将包含 `dma_buf_fd` 的 `v4l2_buffer` 放入 V4L2 队列。
3. **内核映射与 SMMU 转换（dma_buf_attach & dma_map_sg）**：
   - KMD 收到 `dma_buf_fd` 后，在内核中调用 `dma_buf_get(fd)` 获取 `struct dma_buf`。
   - 调用 `dma_buf_attach()` 并使用 `dma_map_sgtable()` 将其映射到 **Camera 专属的 SMMU (IOMMU) 虚拟地址空间**，得到可供 ISP 硬件 DMA 直接访问的 **IOVA (I/O Virtual Address)**。
4. **硬件 DMA 填盘与回调**：
   - ISP (IFE/IPE) 硬件将数据通过 DMA 写入 IOVA。
   - 写入完成后，ISP 触发内核硬件中断（IRQ）。
   - Interrupt Handler 调用 `vb2_buffer_done()`，触发 `VIDIOC_DQBUF` 唤醒 CamX 读取数据。

### 2. 内存大小（Buffer Size）与 Scanline/Stride 计算公式

V4L2 节点分配 Buffer 时，内存大小不仅仅是简单的 `Width × Height × BPP`，必须严格考虑**高通硬件的内存对齐约束（Alignment Constraints）**。

#### 通用计算公式

> GitHub 公式里避免 `_`（即使写在 `\text{}` 中也可能报 `'_' allowed only in math mode`），下面用连字符命名。

$$
\mathrm{BufferSize} = \mathrm{Stride} \times \mathrm{Scanline} \times \mathrm{PlaneCount} + \mathrm{PaddingOrMeta}
$$

其中 `Stride` = Row Pitch（行跨度），`Scanline` = 对齐后的高度。

#### 1) Stride（跨度/行对齐）与 Scanline（列对齐）

高通 ISP 和 GPU 访问内存需要满足特定的字节对齐（通常为 64 字节、128 字节甚至 512 字节对齐），以最大化系统总线（NoC）带宽吞吐。

- **Stride (Width Align)**：每一行像素所占用的实际字节数（包含 Padding）。

$$
\mathrm{Stride} = \mathrm{ALIGN}(\mathrm{Width} \times \mathrm{BytesPerPixel},\ \mathrm{AlignReq})
$$

- **Scanline (Height Align)**：图像高度对齐后的行数。

$$
\mathrm{Scanline} = \mathrm{ALIGN}(\mathrm{Height},\ \mathrm{AlignReq})
$$

#### 2) 常见格式的具体计算示例

**Raw10（MIPI RAW10 压缩/Unpacked 格式）**

MIPI RAW10 在内存中通常每 4 个像素占用 5 个字节（Packing 模式）：

$$
\mathrm{Stride} = \mathrm{ALIGN}\!\left(\frac{\mathrm{Width} \times 10}{8},\ 64\right) = \mathrm{ALIGN}(\mathrm{Width} \times 1.25,\ 64)
$$

$$
\mathrm{TotalSize} = \mathrm{Stride} \times \mathrm{ALIGN}(\mathrm{Height},\ 32)
$$

**NV12（YUV 4:2:0 Semi-Planar，高通最常用的预览/视频格式）**

包含一个 Y 平面和一个交错的 UV 平面（UV 平面高度为 Y 的一半）：

$$
\mathrm{YStride} = \mathrm{ALIGN}(\mathrm{Width},\ 64)
$$

$$
\mathrm{YScanline} = \mathrm{ALIGN}(\mathrm{Height},\ 32)
$$

$$
\mathrm{YSize} = \mathrm{YStride} \times \mathrm{YScanline}
$$

$$
\mathrm{UVSize} = \mathrm{YStride} \times \frac{\mathrm{YScanline}}{2}
$$

$$
\mathrm{TotalSize} = \mathrm{YSize} + \mathrm{UVSize} + \mathrm{UBWCMetaHeader}
$$

等价伪代码（带下划线的命名放在代码块里，GitHub 渲染最稳）：

```text
Y_Stride   = ALIGN(Width, 64)
Y_Scanline = ALIGN(Height, 32)
Y_Size     = Y_Stride * Y_Scanline
UV_Size    = Y_Stride * (Y_Scanline / 2)
TotalSize  = Y_Size + UV_Size + ExtraMetadataHeader   # UBWC / Color Space
```---

## 六、高通通用内存压缩技术：UBWC (Universal Bandwidth Compression)

在高通 SOC（如 Snapdragon 8 系列）上，V4L2 传输的 YUV/RAW Buffer 通常会开启 **UBWC** 标志位：

- **原理**：UBWC 是高通硬件级的无损压缩技术。ISP 写入内存时自动压缩，GPU/Display 读取时自动解压，可节省 30%~50% 的 DDR 内存带宽与功耗。
- **内存影响**：开启 UBWC 后，除了主 Image Buffer 之外，必须额外在 Buffer 尾部（或单独 Plane）分配一块 **UBWC Meta Buffer**（存储每个 Block 的压缩头元数据）。
- **对齐要求**：UBWC 对 Stride 和 Scanline 的对齐要求极高（通常要求 128 字节或 256 字节对齐）。如果 CamX 和 V4L2 KMD 侧的 UBWC Meta Header 对齐尺寸计算不一致，会导致**严重的图像绿条、倾斜或花屏 Crash**。

---

## 七、总结：从 V4L2 看高通 Camera 底层演进

1. **V4L2 是「皮」，KMD/CamX 是「骨」**：高通巧妙地借用了 Linux 标准的 V4L2 设备框架作为接口屏障，底层的核心则是通过 SMMU、dma-buf、CDM 硬件调度器与 ISP 子系统通信。
2. **性能核心在于零拷贝与对齐**：V4L2 的底层性能保障完全依赖 `V4L2_MEMORY_DMABUF` 机制；而排查底层图像问题的关键，90% 都在于校验 **Stride、Scanline 以及 UBWC 内存对齐公式** 是否正确。

---

