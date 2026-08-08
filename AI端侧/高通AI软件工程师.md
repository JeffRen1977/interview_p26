针对高通（Qualcomm）**Senior Staff AI Software Engineer**（通常对应 **L8/L9** 级别的系统架构师与技术领军人）的职位，面试考察的深度和广度会显著超越纯粹的代码实现。除了前面的算法与算子 Coding，面试官（通常是 Principal/Director 级别）还会从以下 **4 个核心维度** 进行全方位考察：

---

## 1. 异构计算与高通硬件架构 (Heterogeneous Computing & HW Architecture)

高通是端侧（On-Device）AI 的霸主，重度依赖其 **Snapdragon (骁龙) 异构芯片**。你必须对高通计算硬件的底层特性和交互机制有深刻理解：

* **Hexagon DSP / NPU 架构与 HVX/HTP**：
* **HTP (Hexagon Tensor Processor)** 的张量加速原理与 Vector Extensions (HVX)。
* 硬件对于 **INT8 / INT4 / FP16 / Microscaling (MX) 数据格式** 的硬件指令支持与算力（TOPS）吞力瓶颈。


* **CPU / GPU / NPU 异构调度与协同 (Heterogeneous Workload Offloading)**：
* 如何根据模型算子特性做 **Sub-graph Partitioning（子图切割与下发）**？例如：哪些算子（如复杂 Attention）放 GPU/CPU，哪些标准算子（如 Conv/MatMul）下发到 NPU？
* **Zero-Copy 内存共享机制**：深入理解 Linux **ION / DMA-BUF** 以及 Android **AHardwareBuffer**，如何做到 CPU/GPU/NPU 内存共享以避免内存拷贝开销。


* **内存带宽与 Cache 瓶颈 (Memory Bandwidth & Cache Hierarchy)**：
* 端侧 AI 最关键的限制不是算力（Compute Bound），而是 **DRAM 内存带宽（Memory/Bandwidth Bound）**。
* 如何利用 **System Cache (LLC)** 和 **L1/L2 Vector Memories (VTCM)** 规避频繁读写 LPDDR 显存。



---

## 2. 深度学习编译器与推理框架 (AI Compiler & Inference Frameworks)

作为一个 Senior Staff 级别的软件工程师，你不仅要会用框架，更要懂得**推理引擎和编译器的内部设计**：

* **Qualcomm AI Stack & QNN (Qualcomm Neural Network) SDK / SNPE**：
* 熟知 QNN 的系统架构：**IR (Intermediate Representation) $\rightarrow$ Graph Transformation / Quantization $\rightarrow$ Backend Execution (HTP/GPU/CPU Driver)**。
* 如何为自定义/未支持的 PyTorch 算子编写 **Custom Op (自定义硬件 Backend 算子)**。


* **Graph-level 编译优化 Pass**：
* 除了常见的算子融合，还包括 **Dead Code Elimination (DCE)、Constant Folding (常量折叠)、Layout Transformation (NCHW $\leftrightarrow$ NHWC 转换消除)、Memory Footprint Minimization (内存生命周期复用)** 等。


* **与开源编译器的对比**：
* 对比 **TVM, TensorRT, ONNX Runtime, ExecuTorch, Apple CoreML** 的设计异同，讨论如何在端侧做跨平台适配。



---

## 3. 端侧大模型 (On-Device LLM & GenAI) 专项优化

高通目前全力推进 **Llama 3 / Mistral / Stable Diffusion** 在骁龙芯片上的本地部署，这一块是当前的重头戏：

* **LLM 推理全流程优化**：
* **Prefill Phase vs. Decode Phase** 的算力与内存瓶颈差异（Prefill 是 Compute-bound，Decode 是 Memory-bound）。
* **KV Cache 管理策略**：PagedAttention 机制在端侧的剪裁与优化；如何在极有限的手机内存（如 8GB/12GB LPDDR5）中挤出空间。
* **Speculative Decoding (投机采样)**：利用轻量级 Draft Model 与主模型配合加速端侧生成速度。


* **极低比特量化 (Ultra-low Bit Quantization)**：
* **INT4 / W4A16 / AWQ / SmoothQuant / GPTQ** 的技术原理与硬件落地方案。
* 如何解决 LLM 在 INT4 量化下的 **Outliers（激活值离群点）** 导致的精度崩塌问题。



---

## 4. 系统架构设计 (System Architecture Design / System Design)

这一轮通常没有代码，面试官会给出一个高概念的开放性题目，评估你的**顶层设计、权衡取舍 (Trade-offs) 与工程领导力**：

* **经典系统设计真题**：
1. *“请为下一代骁龙平台设计一个支持 100 亿参数 LLM + 实时视觉 (Vision-Language) 的通用端侧 AI 推理引擎引擎架构。”*
2. *“如果给你一个完全未知的自定义神经网络，如何自动化地将其部署到 NPU 并保证 99% 的 FP32 精度以及极限延时？”*
3. *“如何设计一个面向摄像头/相机管线 (Camera Pipeline) 的实时 4K@60fps 深度学习 ISP (AI-ISP) 图像处理系统？”*


* **考察重点**：
* **延迟 (Latency) vs. 功耗 (Thermal/Power) vs. 精度 (Accuracy)** 的三方平衡。
* **错误恢复与降级 (Fallback Mechanism)**：当 NPU 内存不足或算子不支持时，如何无缝降级到 GPU/CPU。
* 硬件驱动 (Driver)、HAL 层、C++ 引擎与 Python/Java API 的接口设计。



---

## 5. 技术领导力与项目管理 (Behavioral & Technical Leadership)

Senior Staff 在高通通常需要带项目、制定技术路线图 (Roadmap) 或指导 Junior/Senior 工程师：

* **Cross-Functional Collaboration**：如何与**硬件芯片设计团队 (Hardware Architects)** 沟通？如果硬件团队打算在下一代 NPU 中废除某个指令或增加某个硬件模块，你如何从软件角度给建议？
* **Troubleshooting & Post-Mortem**：分享一个你处理过的最棘手的底层 C++ 内存泄漏、NPU Crash 或多线程死锁/Race Condition 的案例。
* **Tech Roadmap**：如何看待将来 3–5 年端侧 AI 硬件（例如存内计算 Processing-In-Memory、光子芯片、NPU 统一架构）的发展趋势？

---

### 💡 总结备考策略

1. **底层思维**：回答任何软件问题，都要落脚到 **Memory Bandwidth, Cache Line, SIMD Register, Driver Overhead, Thermal Throttling** 等硬件底层概念。
2. **绘制架构图**：在系统设计轮次中，主动画出分层架构图（应用层 $\rightarrow$ API/Framework 层 $\rightarrow$ Graph Compiler $\rightarrow$ Runtime/Engine $\rightarrow$ Driver/HAL $\rightarrow$ HW Cores）。
3. **展现产品化意识**：端侧与云端不同，端侧非常注重 **手机发热 (Thermals) 和电池续航 (Battery Life)**，把“功耗比 (TOPS/Watt)”挂在嘴边会让高通面试官非常赞赏。
