# PICO Vision Algorithm Engineer 面试备考文档

面向字节跳动 PICO **Imaging Systems & Human Engineering** 团队 Vision Algorithm Engineer 岗位。

> **公式显示：** GitHub 仅支持 `$...$`（行内）与 `$$...$$`（独立行）数学语法。请在 GitHub 网页端阅读；本地 Cursor/VS Code 预览可能无法渲染公式。

## 文档目录

| 文档 | 内容 |
|------|------|
| [01-岗位与备考计划.md](./01-岗位与备考计划.md) | JD 解读、简历材料、4 周冲刺计划 |
| [02-PICO面试题库.md](./02-PICO面试题库.md) | 面经汇总、按轮次考点、系统设计题 |
| [03-算法数学公式.md](./03-算法数学公式.md) | conv2d / NMS / 双线性插值 / Tensor 公式与 DL 用法 |
| [04-手撕代码指南.md](./04-手撕代码指南.md) | 手撕题清单、LeetCode、C++、代码索引 |
| [05-系统设计题与模拟面试.md](./05-系统设计题与模拟面试.md) | capture-to-display 标准答案、模拟面试、20 题模板 |
| [06-CV基础题详解.md](./06-CV基础题详解.md) | 通用 CV 面试题详细解答（CNN/检测/分割/Transformer 等） |
| [07-端侧部署题详解.md](./07-端侧部署题详解.md) | 量化/剪枝/蒸馏、推理框架、算子优化、ARM vs NPU |
| [08-影像ISP题详解.md](./08-影像ISP题详解.md) | ISP pipeline、计算摄影、AI-ISP、相机、人眼视觉、端云 |
| [09-追踪交互题详解.md](./09-追踪交互题详解.md) | 裸手追踪、Centaur 手柄融合、面部 blendshape、遮挡恢复 |
| [10-MR感知题详解.md](./10-MR感知题详解.md) | See-through、NeRF/3DGS/SLAM、深度估计、语义分割 |
| [11-影像方向题详解.md](./11-影像方向题详解.md) | 低光增强、C2D 画质、VLM 端云、透视色差模糊（Imaging JD） |
| [12-3D人脸表情追踪详解.md](./12-3D人脸表情追踪详解.md) | 3DMM、Animoji、ARKit Blendshape、ICP、Avatar Retargeting |
| [13-眼动追踪详解.md](./13-眼动追踪详解.md) | 瞳孔检测、glint、gaze 估计、Foveated Rendering、IPD |
| [16-Qualcomm-AI-Stack面试准备.md](./16-Qualcomm-AI-Stack面试准备.md) | Qualcomm AI Stack SDK Staff 岗：技能矩阵、GenAI 推理、mock Q&A |
| [17-AWS-EC2-Nitro-系统设计.md](./17-AWS-EC2-Nitro-系统设计.md) | AWS EC2 Nitro MLS：调度器、GPU 自愈、分布式训练、Host Agent |
| [18-具身智能大模型进展与面试准备.md](./18-具身智能大模型进展与面试准备.md) | VLA 架构、国际/国内模型图谱、benchmark、面试答题 |
| [19-AWS-Nitro-MLS-面试知识点详解.md](./19-AWS-Nitro-MLS-面试知识点详解.md) | Nitro MLS 全考点展开：C++/Linux/EFA/分布式训练/LP/ML 编译器 |

## Amazon C++ 面试（独立目录）

| 文档 | 内容 |
|------|------|
| [../amazon_cpp/README.md](../amazon_cpp/README.md) | 18 Part 索引、示例代码、准备优先级 |
| [../amazon_cpp/docs/](../amazon_cpp/docs/) | 对象模型、STL、并发、Linux、FAQ 详解 |

## NVIDIA DRIVE 面试（独立目录）

| 文档 | 内容 |
|------|------|
| [../nvidia/README.md](../nvidia/README.md) | NVIDIA AV / DriveWorks 备考索引 |
| [../nvidia/01-系统设计.md](../nvidia/01-系统设计.md) | Log ingestion、GPU scheduler、Sensor fusion |
| [../nvidia/02-C++与嵌入式底层.md](../nvidia/02-C++与嵌入式底层.md) | shared_ptr、vtable、volatile、mutex/spinlock |
| [../nvidia/03-AV领域与性能.md](../nvidia/03-AV领域与性能.md) | Hybrid A*、INT8 量化、Amdahl 定律 |
| [../nvidia/04-行为面-One-Team.md](../nvidia/04-行为面-One-Team.md) | 技术分歧、race debug、Why DRIVE |

## 代码练习

手撕参考实现见 [`../interview_handwrite/`](../interview_handwrite/)：

```bash
cd interview_handwrite
python3 conv2d.py && python3 nms.py && python3 bilinear_interp.py && python3 tensor_ops.py
```

## 备考优先级（速查）

| 优先级 | 内容 |
|--------|------|
| **P0** | 王牌项目 dossier、端侧部署、手撕（卷积/NMS/双线性/Softmax） |
| **P1** | ISP / capture-to-display、LeetCode Medium 20–30 题 |
| **P2** | Transformer/VLM、端云协同、PICO 产品与技术文章 |
| **P3** | SLAM / NeRF（简历无则可了解原理） |
