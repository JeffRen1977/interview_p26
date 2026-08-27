# Meta Staff / Senior — 6 大核心主题库（Core Themes）

为应对 Meta Staff / Senior Level 行为面试，将分散素材系统化归纳为 6 大核心主题。每个故事采用 **STAR** 结构，并强化 Staff 级别的：

- **技术领导力（Technical Leadership）**
- **系统性影响力（Systemic Impact）**
- **量化数据（Quantifiable Results）**

原始素材见 [`行为.md`](./行为.md)；2.5–3 min 精修口述稿见 [`meta-staff-star.md`](./meta-staff-star.md)。

---

## 目录索引（故事地图）

| # | 核心主题 | 故事锚点 | 关键数字 |
|---|---------|---------|---------|
| 1 | 战略规划与技术领导力（Strategic Vision & Technical Leadership） | PDAF 多年技术路线图 | JAX 迁移、HWPD 硬件固化、跨 4 代演进 |
| 2 | 最大技术突破与业务成果（Biggest Success & Innovation） | HWPD 芯片面积缩减与 Multi-Depth 架构 | Die Size 削减 **47%**、画质零损失 |
| 3 | 跨团队协同与复杂项目推进（Cross-Functional Collaboration & Driving Execution） | Real Tone 跨团队交付、MLPD 联合开发与 PDAF 校准 | 跨 4 地协同、果断砍 50M |
| 4 | 技术分歧、说服与共识（Conflict Resolution & Influencing Without Authority） | MLPD 引入 LSTM 模块之争 | Hunting 降低 **90%** |
| 5 | 高难度攻坚、模糊性与调试（Handling Ambiguity & Complex Debugging） | AI AF Agent 构建、Remosaic 模糊排查与 DXOMARK 危机逆转 | 准确率 **80%**、提效 **70%**；16x 增益限制 |
| 6 | 工程效率、流程重塑与梯队培养（Engineering Rigor, Process & Mentorship） | 双周快速发版、工具化降本与初级工程师指导 | 双周 Release、AF Simulator、设计文档沉淀 |

---

## 故事 1：战略规划与技术领导力（Strategic Vision & Technical Leadership）

**适用问题：** Tell me about how you set technical roadmaps / How do you drive long-term technical vision?

### Situation

作为 Google Pixel 相机团队的核心技术负责人，随着计算摄影迈入超高变焦（40x–100x）和低光复杂场景，传统的软硬件架构面临功耗、算力瓶颈与低光对焦抖动问题。

### Task

我主导制定了 PDAF 的长期演进路线图（P25–P28），核心目标是在大幅降低功耗的同时，实现 Pixel 相机在业内最高精度与稳定性的自动对焦体验。

### Action

1. **算法演进（Software & ML）：** 推动从传统启发式算法向数据驱动模型（DDM）升级；规划将 MLPD 迁移至 JAX 框架，攻关超长焦与极暗光下的对焦精度。
2. **硬件下沉（Hardware Shift / HWPD）：** 主导将原本由 AP/CPU/GPU 处理的 PDAF 计算逐步固化到专用硬件（HWPD），全面释放系统资源并降低整机功耗。
3. **基础设施与工具化（Infrastructure）：** 建立自动化测试仿真器（AF Simulator）与无人工介入的离线 IQ 评估工具，确保新 Sensor 在 Day-1 即可完成高质量对焦调校。

### Result

该路线图确立了 Pixel 多代旗舰相机的底层对焦技术基线，保障了软硬件跨代际的平滑演进与极致能效。

---

## 故事 2：最大技术突破与业务成果（Biggest Success & Innovation）

**适用问题：** Tell me about your biggest success / Tell me about a time you innovated to solve a hard problem.

### Situation

在定义新一代旗舰 Sensor 与 ISP 硬件架构时，芯片面积（Die Size）和能效预算极其紧张，硬件团队提出需要大幅削减 PDAF 处理单元占用的硅片面积。

### Task

我的目标是在硬件算力与面积大幅裁剪的前提下，通过算法架构创新保证最终画质与对焦精度零倒退。

### Action

1. **算法与硬件联合协同设计（Co-Design）：** 提出 Multi-Depth 与自适应 Masking 算法方案，优化数据吞吐与置信度评估模型（Confidence Model）。
2. **硬件算力重构：** 通过重构底层流水线数据位宽与重用逻辑单元，配合软硬件协同补偿机制，替代原本冗余的高面积硬件模块。
3. **端到端验证：** 带领团队在 FPGA 与仿真平台上进行大量极限场景压测，确保精度损失在算法端被完全吸收。

### Result

成功将 PDAF 硬件芯片面积（Die Size）削减了 **47%**，显著降低了芯片成本与功耗，同时在全量画质与对焦评测中实现了零质量衰减。

---

## 故事 3：跨团队协同与复杂项目推进（Cross-Functional Collaboration & Driving Execution）

**适用问题：** Tell me about a complex project you led across multiple teams / How do you drive execution across XFN?

### Situation

在 Pixel Real Tone 与跨代 Camera 交付中，涉及 Mountain View、San Diego、Taipei、Korea 多地的 Software、Algorithm、Hardware、Sensor、ISP、PCIQ（画质评测）以及 PM 团队，跨职能、跨地域协同复杂度极高。

### Task

我负责拉通 PDAF 核心链路，确保跨团队接口清晰，按期交付高品质算法并满足产品发布标准。

### Action

1. **明确契约与测试规范：** 针对 Real Tone 与新 Sensor 校准，为 PCIQ 团队制定结构化测试规范，将主观反馈转化为可度量的缺陷指标，并自动归类分配给内部 3A 团队。
2. **跨团队攻关新 Sensor 格式：** 面对新 Sensor 格式带来的校准挑战，在极短交付窗口期内协助 TechEng 团队深入排查根因，调整校准算法，大幅缩短产线校准耗时。
3. **严控交付风险与果断决策：** 在 50M/Remosaic 模式评估中，基于严格的数据评测发现其画质无法达到量产线标准，果断推动 Scope Cut，避免资源浪费。

### Result

保障了 Real Tone 等核心旗舰功能按时高标准交付，并建立了跨地域、跨职能的标准协作交付模板。

---

## 故事 4：技术分歧、说服与共识（Conflict Resolution & Influencing Without Authority）

**适用问题：** Tell me about a time you had a technical disagreement / How do you influence others?

### Situation

在开发 MLPD（机器学习相位对焦）算法时，模型在复杂动态场景下存在对焦振荡（Hunting）问题。我提出引入 LSTM 模块利用时序信息（Temporal Filtering）平滑轨迹，但合作的 TechEng 团队坚决反对，认为这会增加计算功耗并需要推倒重新训练模型。

### Task

我需要在不激化团队矛盾的前提下，基于数据和客观事实打消对方顾虑，就模型架构升级达成共识。

### Action

1. **对齐目标与量化约束：** 与对方深入沟通，明确核心争议在于“计算开销与训练成本”对比“画质稳定性收益”。
2. **快速 POC 与轻量化设计：** 带领团队用数天时间构建微型 POC，在时序模块中引入轻量化设计，并通过模型量化将算力开销严格控制在 ISP/NPU 预算之内。
3. **数据说话与 A/B 测试：** 将两套方案在严苛晃动及低对比度场景下进行 Side-by-Side 盲测与性能数据比对，清晰证明时序滤波带来的稳定收益。

### Result

彻底化解分歧并达成共识，模型最终使对焦振荡（Hunting）减少了 **90%**，助力 Pixel 相机在当期评测中斩获领先成绩。

---

## 故事 5：高难度攻坚、模糊性与重大风险修复（Ambiguity, Risk & Debugging）

**适用问题：** Tell me about a time you handled ambiguity / Tell me about a time you fixed a complex bug or managed high risk.

### 场景 A：应对模糊性（Handling Ambiguity）——构建 AI AF Triage Agent

#### Situation

初期尝试使用大模型辅助对焦 Bug 分流与根因定位时，模型因缺乏底层物理规律与相机光学认知，诊断准确率极低，输出模糊且不可用。

#### Task

在无先例可循的情况下，探索如何让 AI Agent 具备资深工程师级别的对焦分析判断力。

#### Action

1. **门禁与预处理：** 设计 Ingest 框架，对缺少 Sensor Raw Dump 或关键视频日志的 IQ Ticket 自动打回并提示规范抓取。
2. **知识图谱与模块化 Skill：** 构建专用的 AF Knowledge Graph，并驱动各子团队将调试经验抽象封装为独立的 Agent Skills（如 PD 信号分析、执行器控制分析等）。
3. **仿真闭环：** 将 AF Simulator 无缝挂载为 Agent 工具，使 Agent 能自主回放 Trace 并提取低层收敛曲线。

#### Result

Agent 达到 **80%** 的根因诊断准确率，为工程师节省了 **70%** 的分流耗时。

### 场景 B：重大风险逆转与深度调试（High-Risk Crisis & Complex Debugging）——DXO 回归与 Remosaic 缺陷排查

#### Situation

在一次关键评测（DXO#2）中，相机对焦出现严重性能回退（Regression），得分远低于预期，直接威胁到量产发布节点。

#### Task

作为技术专家，我受命在极短时间内组织跨团队攻坚，排查根因并逆转评测劣势。

#### Action

1. **全链路 Sweep 与差异对比：** 对比算法版本与历史机型，确认算法逻辑完全一致；随后深入 Sensor RAW 层做逐帧 Dump 与全范围扫描（Full Sweep）。
2. **跨层根因定位：** 发现 Sensor 模拟增益（Analog Gain）在初期 Bring-up 配置中被错误限制在 **16x**，导致弱光下 PD 信号信噪比严重不足引发误判。
3. **快速止血与建立防线：** 紧急修正寄存器配置并重设 PD 置信阈值；同时将 RAW 增益校验加入自动化 Bring-up 门禁列表，杜绝同类隐患。

#### Result

在随后的复测（DXO#3）中成功逆转并取得优异评分，消除了重大发布风险。

---

## 故事 6：工程效率、流程重塑与梯队培养（Engineering Rigor, Process & Mentorship）

**适用问题：** How do you improve team efficiency / How do you mentor junior engineers?

### 工程流程变革（Process & Efficiency）

1. **发版机制重构：** 针对最初 Google3 研发与 Android 体系脱节、依赖人工 Cherry-pick 的低效模式，推动团队转向双周滚动式快速发布（Two-Week Rapid Release Roll-up）机制，大幅提升代码迭代与交付敏捷度。
2. **自动化离线工具：** 研发离线图像自动化评测工具与 AF Simulator，摆脱纯人工肉眼低效看图评测，提升测试客观性与吞吐量。
3. **高效会议文化：** 推行 30 分钟精简会议规范（会前明确 Agenda、最小化参会人、会后即时 Action Items 跟踪）。

### 技术梯队培养（Mentorship & Tech Leadership）

1. **标准化知识沉淀：** 编写系统级 Architecture & Design Documents 与完善的 Onboarding 知识库，降低新人上手门槛。
2. **聚焦 1:1 辅导：** 通过定期的 1:1 技术交流，帮助初级工程师拆解技术目标、明确系统边界并引导其进行规范的代码设计评审（Design Review）。

---

## 面试即时检索与速查矩阵（Cheatsheet）

| 面试问法 / 考点 | 对应最佳故事索引 | 关键数字与闪光点 |
|----------------|-----------------|----------------|
| Technical Roadmap / Long-Term Vision | 故事 1：PDAF 长期规划 | JAX 迁移、HWPD 硬件固化、跨 4 代演进 |
| Proudest Achievement / Innovation | 故事 2：HWPD 芯片优化 | Die Size 削减 **47%**、画质零损失 |
| XFN Collaboration / Stakeholder Management | 故事 3：Real Tone 跨团队交付 | 跨 4 地协同、PCIQ 需求结构化、果断砍 50M |
| Technical Conflict / Disagreement | 故事 4：MLPD LSTM 之争 | Hunting 降低 **90%**、轻量化 POC 数据服人 |
| Ambiguity / AI Exploration | 故事 5A：AI AF Agent | 准确率 **80%**、提效 **70%**、Knowledge Graph |
| Complex Bug / Failure & Recovery | 故事 5B：DXO 逆转与 Sensor 排查 | 深入 RAW 排查 **16x** 增益限制、建立 Bring-up 防线 |
| Team Efficiency / Mentorship | 故事 6：双周发版与工程文化 | 双周 Release 闭环、AF 自动化仿真、设计文档沉淀 |
