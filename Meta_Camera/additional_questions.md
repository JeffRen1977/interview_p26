# Meta Embedded Camera — 缺口分析与补充出题预测

对 [`camera_software_engineer_prep.md`](./camera_software_engineer_prep.md)、[`coding_interview_guide.md`](./coding_interview_guide.md)、[`code/`](./code/)、[`camera_system_design/`](./camera_system_design/) 的一次红队审查。

**已有的部分很扎实**：四个系统设计场景、ISP/3A 深度、功耗热约束的数字都在。下面只写**没覆盖到、但很可能被问**的东西。

---

## 0. 结论先行

| 缺口 | 严重度 | 已补 |
|------|--------|------|
| 清单列了但没有可跑代码的题（crop/rotate、NV12、topo sort） | 高 | ✅ [`code/`](./code/) 新增 10 题 |
| **相机–IMU 时间戳对齐**（Meta tracking 第一性问题，清单完全没有） | **最高** | ✅ [`code/timestamp_sync.md`](./code/timestamp_sync.md) |
| **IR LED 连通域 + 亚像素质心**（controller tracking 核心，完全没有） | **最高** | ✅ [`code/blob_centroid.md`](./code/blob_centroid.md) |
| **流式包解析器**（嵌入式电面最高频的非 LeetCode 题，完全没有） | **高** | ✅ [`code/packet_parser.md`](./code/packet_parser.md) |
| 定点数学 / 无 FPU（只在表格出现一次） | 中高 | ✅ [`code/fixed_point.md`](./code/fixed_point.md) |
| **嵌入式 C/C++ 快问快答**（volatile、cache coherency、ISR 约束） | **高** | ✅ 本页 §2 |
| **调试轮**（"百帧掉一帧，怎么查"这种格式） | **高** | ✅ 本页 §3 |
| Linux / V4L2 / HAL 驱动栈问答 | 中高 | ✅ 本页 §4 |
| 系统设计缺的三个场景（眼动、隐私仲裁、EIS/passthrough） | 中 | ✅ 本页 §5 |
| 行为面的 Meta 特定信号 | 中 | ✅ 本页 §6 |

---

## 1. Coding：还该会但本仓库暂时只有口述版的题

已补的 10 题见 [`code/README.md`](./code/README.md)。下面这些**没写代码**，但要能在白板上 15 分钟内出手：

| 题 | 一句话解法 | 相机语境（拿来开场） |
|----|-----------|---------------------|
| **双线性 resize / remap** | 4 邻域加权，权重用 Q8 定点；坐标先算再 clamp | 镜头去畸变（undistort）就是一张 remap 表 + 双线性采样。追问：为什么用**反向映射**（从 dst 找 src）而不是正向——正向会留空洞 |
| **坏点检测与修复** | 3×3 中值，或"与 8 邻域同色通道比较超阈值则替换" | BPC（Bad Pixel Correction）在 ISP 最前端。Bayer 上要**跳格取同色邻居**，不能取相邻像素 |
| **NMS（非极大值抑制）** | 3×3 局部极大 + 阈值；框级 NMS 按分数排序 + IoU 剔除 | FAST/Harris 角点提取后必跑。仓库有 Python 版 [`interview_handwrite/nms.py`](../interview_handwrite/nms.py)，**要会 C++ 版** |
| **滑动窗口最大/最小值** | 单调双端队列，O(N) | 曝光/增益序列的包络、AE 防抖的历史窗口 |
| **数据流中位数** | 双堆（大顶+小顶）平衡；或定长直方图（8-bit 只有 256 桶，更适合嵌入式） | 时间中值降噪、丢帧率统计的 p50 |
| **LRU（按字节容量而非条目数）** | 哈希 + 双向链表，`evict while (bytes > cap)` | ZSL 环形缓存里按内存预算淘汰帧 |
| **1 生产者 N 消费者分发** | SPSC 每消费者一条 + 引用计数帧，而不是一条 MPMC | 一帧同时给 preview / encoder / CV。接 [`code/frame_pool.md`](./code/frame_pool.md) |
| **两个有序数组求交/并** | 双指针 | 多相机的共视帧配对 |
| **位图（bitmap）分配器** | `__builtin_ctzll` 找第一个空位 | 小块 buffer 或 DMA channel 分配 |
| **RGB565 / RGB888 打包解包** | 掩码 + 移位，注意 5-6-5 的**位扩展**（`(v<<3)|(v>>2)` 而不是 `v<<3`） | 显示路径格式转换。RAW10 的姊妹题 |

**位扩展这一点值得记住**：5-bit → 8-bit 用 `(v << 3) | (v >> 2)`，保证 `31 → 255` 而不是 `248`。面试官很爱追这个。

---

## 2. 嵌入式 C/C++ 快问快答

电面 coding 之间的填空题，答错一个很扣分。**每条限 30 秒答完。**

### 内存与硬件

| 问 | 答 |
|----|----|
| `volatile` 和 `std::atomic` 区别？ | `volatile` 只禁止**编译器**优化掉访问（用于 MMIO 寄存器），**不提供任何原子性和内存序**。跨线程共享必须用 `atomic`。用 `volatile` 做线程同步是错的 |
| MMIO 寄存器怎么写？ | `volatile uint32_t*`，读-改-写要注意**写 1 清零（W1C）**和只写寄存器；有些寄存器读会有副作用，不能随便读 |
| DMA 前后要做什么？ | CPU 写 → 设备读：先 **clean/flush** cache；设备写 → CPU 读：先 **invalidate**。漏一个就是偶发花屏/撕裂。Linux 里是 `dma_sync_single_for_device/cpu` |
| 结构体 `sizeof` 为什么大于成员之和？ | 对齐 padding。`#pragma pack(1)` 能去掉，但**非对齐访问在部分 ARM 上会 fault 或变慢**。协议解析用逐字节读+移位，不要直接 cast 结构体指针 |
| 位域（bitfield）能用来解析寄存器吗？ | 不能可移植地用。**位域的排布顺序是实现定义的**（大小端、填充方向都不保证）。用移位掩码 |
| 大小端影响什么？ | 网络/文件格式、跨芯片共享内存、**位域顺序**。同一个芯片内的普通变量不受影响 |
| 栈上能放多大？ | 嵌入式线程栈常见 4–16 KB。**图像 buffer 绝不能放栈上**，一个 VGA 帧就爆了 |
| `restrict` 有什么用？ | 告诉编译器指针不别名，能让它向量化和重排。`memcpy` 的签名有，`memmove` 没有——这正是两者的区别 |

### 并发

| 问 | 答 |
|----|----|
| ISR 里能做什么？ | **不能**：阻塞、`malloc`、加会睡眠的锁、printf、浮点（除非保存了 FPU 上下文）。**只做**：读寄存器、清中断标志、置标志/推无锁队列、唤醒线程 |
| 顶半部/底半部？ | 顶半部在 ISR 上下文，最小化；底半部是 softirq / tasklet / **workqueue**（可睡眠）/ 内核线程。相机 SOF 中断打时间戳属于顶半部，buffer done 处理放底半部 |
| ISR 和线程共享变量要什么？ | `atomic` 或关中断的临界区。**不能用 mutex**（ISR 不能睡）。单核上关中断，多核上还要自旋锁 |
| acquire / release 到底保证什么？ | release store 之前的所有写，对看到这个 store 的 acquire load 之后的读可见。SPSC 里：先写 payload，再 release 发布 index |
| 为什么 relaxed 不行？ | 编译器和 CPU 可以把 payload 的写重排到 index 发布之后，消费者会读到未初始化的 buffer |
| false sharing 是什么？ | 两个线程写同一 cache line 的不同变量，line 在核间来回弹。SPSC 的 head/tail 要 `alignas(64)` 分开 |
| 优先级反转？ | 低优先级持锁挡住高优先级。解法：优先级继承（priority inheritance）或天花板协议。相机上更常见的是"预览线程被日志线程的锁挡住导致掉帧" |
| 条件变量为什么必须用 while？ | 虚假唤醒（spurious wakeup）+ 唤醒后条件可能已被别的消费者改变 |

### C++ 在嵌入式的取舍

| 问 | 答 |
|----|----|
| 为什么关 exceptions / RTTI？ | 代码体积（unwind 表）、**抛出路径延迟不确定**（实时路径不可接受）。用错误码 / `expected` |
| 为什么禁动态内存？ | 碎片化 + 延迟不确定。启动时分配好池，运行时只借还（见 [`code/frame_pool.md`](./code/frame_pool.md)） |
| 虚函数能用吗？ | 能，但热路径上每次调用一次间接跳转 + 可能的 icache miss。ISP 节点接口用虚函数没问题，**per-pixel 回调不行**（用模板/CRTP） |
| 基类析构函数为什么要 virtual？ | 通过基类指针 delete 派生对象否则 UB。或者干脆禁止多态删除（protected 非虚析构） |
| 静态初始化顺序问题？ | 跨 TU 的全局对象构造顺序未定义。用函数内 static（Meyers singleton）或显式 `init()` |
| RAII 在嵌入式的价值？ | buffer 借还、锁、fd、dma-buf 引用——所有配对操作。这是 C++ 比 C 值的地方，要主动说 |
| placement new 什么时候用？ | 在池的内存上构造对象，避免 `operator new`。配对手动调析构函数 |

---

## 3. 调试轮：Meta 一定会问的题型

这个格式在你的准备里只作为行为题出现过（"最难的 bug"）。但它也会作为**技术题**出现，考的是有没有真的调过：

### "预览每 100 帧掉 1 帧，你怎么查？"

**答题结构：自顶向下分层定位，每层说出用什么工具看。**

```
1. 先确定"掉"在哪一层 —— 每层都有帧计数器
   Sensor 出了吗？    → sensor 的 frame counter 寄存器 / SOF 中断计数
   CSI 收到了吗？     → CSID 的 SOF/EOF 计数、CRC 错误计数、overflow 状态位
   IFE 写出了吗？     → buffer done 中断计数
   HAL 交付了吗？     → CaptureResult 的 frame number 序列有没有洞
   App 拿到了吗？     → Surface 的 dropped frame 统计
   显示上屏了吗？     → SurfaceFlinger / compositor 的 late frame

   哪一层的计数第一次对不上，问题就在那一层和上一层之间。
```

**然后按层给根因假设：**

| 层 | 典型根因 | 证据 |
|----|----------|------|
| Sensor | 曝光时间 > 帧周期，自动降帧；PLL/mode 配置错 | 计算 `exposure vs 1/fps`；看 sensor mode 表 |
| MIPI | 信号完整性、lane 数不够、CRC/ECC 错、**CSID overflow**（下游来不及收） | CSID 错误寄存器、示波器/眼图、降 lane rate 试 |
| DDR 带宽 | 编码器 + 显示 + ISP 同时抢带宽，ISP 写 buffer 超时 | 带宽 profiler；把 encoder 关掉复现 |
| Buffer | 池耗尽（消费者持有太久没还） | 池的 in-use 高水位；哪个消费者 hold 住了 |
| 调度 | 处理线程被抢占/被锁挡住超过一帧周期 | **Perfetto / systrace / ftrace**，看线程 runnable 到 running 的延迟 |
| 热 | 降频后来不及 | thermal zone 温度曲线与掉帧时刻对齐 |

**加分句：**
- "1/100 而不是随机——先看**周期性**。如果正好每 N 帧一次，多半是某个定时任务（3A 收敛、统计上报、GC、日志刷盘）撞上来了。"
- "我会先加**不改变时序的**观测：ftrace / 硬件计数器，而不是 printf——printf 本身会改变时序，把 bug 藏起来（heisenbug）。"
- "复现条件要压到最小：固定曝光、关 3A、关编码，一个一个加回来。"

### 其他高频调试题

| 题 | 关键词 |
|----|--------|
| **画面偶发撕裂/花屏一条** | DMA cache 一致性；buffer 在 producer 还没写完就被 consumer 读（缺 fence）；stride 算错 |
| **图像整体偏绿/偏紫** | Bayer pattern 认错（RGGB vs BGGR）；CCM 或 AWB 增益没应用；黑电平减错 |
| **图像斜切（skew）** | stride 用成了 width；或 RAW10 packed 当 unpacked 解 |
| **暗部有横条纹** | 黑电平不均 / 行噪声；或 gamma LUT 用了截断（见 [`code/fixed_point.md`](./code/fixed_point.md)） |
| **AE 来回震荡（hunting）** | 控制环增益太高、没有阻尼/迟滞；或者作用延迟算错了（曝光生效在 **N+2** 帧，不是下一帧） |
| **头动时 passthrough 有果冻感** | rolling shutter + 缺少逐行时间戳补偿；或 pose 用的是 SOF 而不是曝光中点 |
| **启动后前几帧过曝/过暗** | 3A 还没收敛；要么丢弃前 N 帧，要么用上次的 AE 结果做种子（persist 到 NVM） |
| **长时间录像后掉帧变多** | 热降频；或内存碎片化/泄漏（buffer 没还）；或文件系统写入抖动 |
| **只在某一台设备上复现** | 标定数据（per-unit）、sensor 批次差异、模块装配公差。先比对两台的 calibration blob |

---

## 4. Linux / V4L2 / HAL 驱动栈

如果 JD 里有 "kernel"、"driver"、"BSP"、"bring-up"，这一轮跑不掉。

| 问 | 答 |
|----|----|
| V4L2 里相机是怎么建模的？ | **media controller** 拓扑：sensor subdev → CSI receiver subdev → ISP subdev → video node。`media-ctl` 配链路，`v4l2-ctl` 配格式和跑流 |
| subdev 和 video node 区别？ | subdev 是**没有 buffer 的处理单元**（只有 pad 和格式协商），video node 是**有 buffer 队列的端点**（`VIDIOC_QBUF/DQBUF`） |
| buffer 有几种传递方式？ | `MMAP`（驱动分配，mmap 到用户态）、`USERPTR`（用户分配）、**`DMABUF`**（fd 传递，零拷贝跨设备共享）。相机零拷贝走 DMABUF |
| dma-buf fence 解决什么？ | 生产者写完前消费者不能读。`explicit fence` 让 ISP → GPU → display 串起来**不用 CPU 参与同步**，这是 glass-to-glass 延迟的关键 |
| sensor 驱动 probe 做什么？ | 从 device tree 拿 I2C 地址、时钟、GPIO(reset/pwdn)、regulator、lane 数；上电时序 → 读 chip ID 校验 → 注册 subdev 和 v4l2 controls |
| sensor 寄存器怎么配？ | I2C（Qualcomm 上是 **CCI** 硬件 I2C 控制器）。模式切换是一大张寄存器表；曝光/增益是每帧下发，且要**分组写（group hold）**保证同一帧生效 |
| 曝光生效延迟？ | 典型 **N+2**：这一帧下发，下一帧 sensor latch，再下一帧才输出。3A 控制环必须建模这个延迟，否则震荡 |
| device tree 里描述什么？ | 硬件拓扑：I2C 总线和地址、CSI 端口和 lane 映射、时钟频率、GPIO、endpoint 互连（`ports`/`endpoint`） |
| 中断处理流程？ | `request_irq` → 顶半部读状态寄存器、清中断、打时间戳 → 唤醒 threaded IRQ 或 workqueue 做 buffer done |
| 怎么调 kernel 侧问题？ | `dmesg`、`ftrace`/`trace_printk`、`v4l2-dbg` 读寄存器、`media-ctl -p` 打拓扑、逻辑分析仪看 I2C、示波器看 MIPI 和 FSYNC |
| Android HAL3 的核心契约？ | **每个 CaptureRequest 恰好对应一个 CaptureResult**，且 result 里带回实际生效的 metadata（曝光、增益、时间戳）。乱序 / 丢 result 就是框架级 bug |
| Camera HAL 的 buffer 从哪来？ | 上层 Gralloc 分配（`ANativeWindow`），以 buffer handle 下发给 HAL，HAL 填完带 release fence 还回去 |

---

## 5. 系统设计：补充场景（已全部写成完整篇）

[`camera_system_design/`](./camera_system_design/) 原有四篇（tracking 同步、E2E ISP、功耗热、AI-ISP）已**升级到 E6 深度**——每篇加了 §6–§10：数字预算（当场算并收口）、关键决策与被否方案、失效模式与降级、**怎么证明它是对的**、演进与组织落地。

另外补了五个原本完全没有的场景：

| # | 场景 | 一句话核心 |
|---|------|-----------|
| [05](./camera_system_design/05-eye-face-tracking.md) | 眼动 / 面部追踪与 Foveated Rendering | glint 消除相机-眼睛的相对位移自由度；**子系统必须自证 ROI**（260 mW 换 1.2 W GPU 节省）；最大风险是**人群覆盖**而不是精度 |
| [06](./camera_system_design/06-passthrough-reprojection.md) | Passthrough 重投影与 EIS | 核心矛盾是**相机不在眼睛的位置**（0.3 m 处 9.5° 视差）；延迟预算一半由帧率和显示锁相决定；失败是**安全问题**，永不黑屏 |
| [07](./camera_system_design/07-camera-arbitration-privacy.md) | 多客户端仲裁与隐私 | 两个正交轴：仲裁是工程问题（抢占延迟最大项是 3A 收敛），**信任不是**——隐私灯必须硬件绑定，且必须在硬件设计评审期推动 |
| [08](./camera_system_design/08-calibration-system.md) | 标定系统（产线 → 在线 → OTA） | 骨架是**按变化尺度分四类参数**；把节拍翻译成资本支出；建立从 fleet 回到产线的 SPC 反馈回路 |
| [09](./camera_system_design/09-validation-infrastructure.md) | 相机验证与测试基础设施 | **E6 专属题**。基石是 bit-exact 回放框架（ROI 8×）；元指标是**逃逸归因** |

**第 09 篇尤其值得准备**：它是唯一一道你可以**主动引出**的题——在任何设计题的"怎么验证"环节说"我会建立 X"，面试官很可能顺势深挖。而绝大多数候选人在这一层是空白的。

答题框架、E5/E6 分界线、以及**白板速算表**（带宽 / MIPI / 功耗 / 延迟 / 热 / 能量的现算公式）见 [`camera_system_design/README.md`](./camera_system_design/README.md)。

---


## 6. 行为面：Meta 特有的信号

你的 STAR 槽位（5 个）没问题。补三个**Meta 特别会挖**的角度：

| 角度 | 问法 | 要给出的东西 |
|------|------|-------------|
| **Move fast 与质量的张力** | "讲一次你为了赶节点做了技术妥协。" | 妥协的**范围界定**（哪些不能妥协：安全、隐私、数据正确性）、**还债计划**和它真的被还了的证据。不要讲成"我从不妥协" |
| **数据驱动而非资历驱动** | "你怎么说服一个比你资深的人？" | 用**可复现的实验**推翻直觉，而不是辩论。给出你搭的度量工具本身（这在 Meta 是加分项：造工具 > 赢辩论） |
| **规模与外部性** | "你的改动影响了多少人/设备？出问题怎么办？" | 灰度、kill switch、回滚时间、影响面估算。硬件产品**不能像服务一样回滚**——要说清 OTA 的现实约束 |

**还要准备一个"我做错了"的故事**：Meta 很看重能不能诚实地讲失败并说出系统性改进（不是"我熬夜修好了"，而是"我们加了 X 让这类问题不会再发生"）。

**数字化每一个故事**：ms、mW、掉帧率、PSNR/SSIM、崩溃率、覆盖设备数。你的准备里已经写了这条，但要真的对每个故事都填上数字——面试当场编不出来。

---

## 7. 反问面试官（每轮留 3–5 分钟）

选和你的岗位假设最相关的问：

- 这个岗位偏 **tracking/CV 数据通路** 还是 **media/IQ**？团队和 sensor HW / 算法团队的边界在哪？
- 你们的相机栈是基于 Android Camera HAL3，还是自研 RTOS/Linux 上的定制栈？
- 新 sensor bring-up 一般多久？谁写 sensor driver、谁做 IQ tuning？
- 功耗和热的预算是怎么在团队间分配的？有没有一个所有人共用的度量平台？
- 眼镜和头显这两条产品线的相机代码复用程度如何？
- 现在最痛的技术债是什么？

---

## 8. 一周执行顺序建议

现有的执行清单在 [`camera_software_engineer_prep.md`](./camera_software_engineer_prep.md) §5。把新增内容插进去：

| 天 | 做什么 |
|----|--------|
| D1 | 闭卷写 [`code/`](./code/) 原有 7 题（RAW10 / rotl / endian / aligned_malloc / memmove / box / histogram），每题计时 18 分钟 |
| D2 | 闭卷写新增图像题：strided crop+rotate、NV12→RGB、Bayer demosaic、integral image |
| D3 | 闭卷写 **timestamp_sync** 和 **blob_centroid**（这两题最可能出现且你之前没练过） |
| D4 | 闭卷写 **packet_parser** 和 **frame_pool**；然后过一遍本页 §2 快问快答，答不出的当场补 |
| D5 | 系统设计：抽 [`camera_system_design/`](./camera_system_design/) 一篇，12 分钟画完 + 讲一个深挖点；再口述本页 §5 的三个新场景各一遍 |
| D6 | 调试轮（本页 §3）：把"百帧掉一帧"完整讲三遍，直到分层定位不用想；过 §4 驱动栈问答 |
| D7 | STAR 五个故事各讲一遍并**填上数字**；准备反问；只复习白板口令，不写新代码 |
