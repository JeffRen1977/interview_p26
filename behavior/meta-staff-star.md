# Meta Staff STAR — Camera AF 四则口述稿

草稿案例的 **Staff / Senior** 精修版。重点是技术深度、主导权（Technical Leadership & Ownership）、架构权衡与 XFN 协同，用可量化的英文讲。

原始素材见 [`行为.md`](./行为.md)。

| # | Prompt | 故事锚点 | Meta 价值观 |
|---|--------|----------|-------------|
| 1 | Most impactful project you led | AI-Powered AF Triage Agent | Focus on Long-Term Impact + Move Fast |
| 2 | Technical disagreement | LSTM temporal AF vs more data | Be Direct and Respectful + data-driven consensus |
| 3 | High ambiguity | Generic LLM → production AF agent | Live in the Future + navigating ambiguity |
| 4 | Failure / mistake | Low-light DXO AF regression | Be Open + systemic guardrails |

**时间：** Situation 30s → Task 20s → Action 90s → Result 30s，总长 **2.5–3 分钟**。  
**人称：** Action 用 *I initiated / I architected / I benchmarked*；协同和结果处用 *We*。

---

## 1. Tell me about the most impactful project you led.

**Situation.** In our Camera AF team, we handled hundreds of incoming bug reports daily across multiple hardware platforms. Manually triaging logs, reproducing traces via AF simulators, and identifying root causes consumed substantial engineering bandwidth, creating a major triage bottleneck.

**Task.** As the Tech Lead, I initiated and architected an end-to-end **AI-Powered AF Triage Agent** to automate multi-modal log ingestion, simulation-based root cause analysis, and automated assignment.

**Action.**

1. *Multi-modal data ingestion.* Built an automated pipeline to parse Buganizer tickets, extracting structured metadata (titles, comments) along with raw diagnostic artifacts (sensor dump logs, AF simulation traces, and test images/videos).
2. *Autonomous simulation and analysis.* Integrated our proprietary AF simulator into an agentic workflow. The agent executes targeted traces, extracts low-level convergence metrics, and feeds combined context into an LLM reasoning engine equipped with domain-specific diagnostic prompts.
3. *Closed-loop routing.* Designed an automated decision head that predicts the root cause, maps it to specific sub-modules (e.g. PD sensor calibration, low-light hunting, actuator control), and dispatches the ticket directly to the appropriate Point of Contact (POC).

**Result.** Achieved **80% root-cause triage accuracy** across production bugs, reducing engineering triage time by **70%** (hundreds of engineering hours per quarter) and accelerating overall bug turnaround by **2.5×**.

---

## 2. Tell me about a time you had a technical disagreement with a colleague or stakeholder.

**Situation.** During a major camera generation cycle, our ML-based AF algorithm suffered from persistent focus hunting in low-contrast and dynamic scenes. The algorithm team (Techeng) insisted that the model topology was sufficient and simply wanted to collect more sensor data to retrain the legacy baseline.

**Task.** I recognized that the hunting stemmed from a lack of temporal continuity in the current single-frame model. I needed to convince the team to adopt a temporal architecture (LSTM/GRU) without delaying the product launch schedule.

**Action.**

1. *Depersonalized 1:1 and active listening.* Understood their valid concerns: increased computational cost against the ISP/NPU latency budget, and the overhead of retraining from scratch.
2. *Data-driven POC and benchmark.* Instead of debating theory, I spent 3 days building a lightweight POC with an LSTM module. I benchmarked it on our hardware simulator and applied INT8 quantization to keep inference latency within our **1.2 ms** frame budget.
3. *Side-by-side evaluation.* Ran an A/B test on challenging benchmark sequences, showing that temporal state memory stabilized the AF lens trajectory without significant memory or compute penalties.

**Result.** We aligned on the temporal model architecture, which **reduced AF hunting by 90%**, eliminated lens oscillation in low light, and helped our flagship camera achieve the **highest DXOMARK autofocus score** for that product generation.

**Trade-off to say out loud.** More data on a single-frame model would not create temporal continuity; an unquantized LSTM would miss the 1.2 ms budget. The POC made both claims falsifiable in three days.

---

## 3. Tell me about a time you handled a high level of ambiguity.

**Situation.** When first prototyping an LLM agent for camera AF triage, the initial prototype showed severe hallucination and poor diagnostic precision. AF debugging relies on domain-specific physics (PD confidence, optical transfer functions, sensor gain) that generic LLMs lack, so the path from a generic agent to human-expert-level judgment was undefined.

**Task.** I took full ownership of resolving this ambiguity and established a structured methodology to bridge low-level camera physics with agentic reasoning.

**Action.**

1. *Requirement and gatekeeping filtration.* Designed a pre-triage ingestion framework. If a ticket was flagged as an Image Quality (IQ) issue but lacked raw dumps or sensor video traces, the agent bounced it back with precise data-collection instructions.
2. *Modular skill framework.* Architected a modular Skills ecosystem: standardized tool interfaces, and partnered with sub-teams (Sensor, 3A/ISP, Actuator Driver) so each domain owned a self-contained diagnostic skill.
3. *AF knowledge graph.* Mapped low-level hardware failure modes, ISP metadata, and error codes to human-vetted debugging workflows.
4. *Empirical iteration.* Deployed an experimental agent (Orcas) against a golden dataset of 100 historical complex bugs, iteratively refining prompt grounding and retrieval.

**Result.** Turned a vague AI initiative into a deterministic, production-grade architecture, lifting triage accuracy to **80%** and establishing the standard multi-agent debugging pattern adopted across the broader camera organization.

**Ambiguity move.** Do not prompt the model harder. Constrain the inputs (gatekeeping), give it tools (skills), and ground it in a graph of real failure modes.

---

## 4. Tell me about a time you failed or made a mistake. What did you learn?

**Situation.** During final validation for a new flagship smartphone, we found an unexpected **AF regression in low-light DXOMARK benchmarks**: focus acquisition latency was significantly worse than the prior generation.

**Task.** As the lead engineer for AF performance, I initiated a blameless RCA to unblock the release schedule.

**Action.**

1. *Comparative baseline.* Verified that core AF algorithmic logic matched the previous generation, isolating the issue from an algo regression.
2. *Deep RAW and sensor dump.* Compared raw sensor frames under identical lux: the new generation's inputs were noticeably darker with higher noise.
3. *Cross-functional investigation.* Partnered with Sensor and ISP. The new sensor driver was artificially capped at **16× maximum analog gain** from an incomplete configuration override during early bring-up, starving Phase Detection of SNR.
4. *Mitigation and guardrails.* Adjusted analog-gain registers and recalibrated the PD confidence threshold to restore AF performance.

**Result.** We resolved the regression before mass production. The real lesson: black-box sensor assumptions were a critical gap. I established a mandatory **Pre-Bringup Sensor & ISP Sanity Checklist** in the CI/CD validation pipeline so analog-gain limits and RAW exposure curves are verified on day one of future hardware bring-up.

**What I owned as the failure.** I shipped AF on an unstated sensor contract. The systemic fix is the checklist, not "we found a driver bug."

---

## Delivery

| 段 | 时长 | 不要做 |
|----|------|--------|
| Situation | 30s | 讲团队史 |
| Task | 20s | 把目标说成 "we needed to improve quality" |
| Action | 90s | 清单式罗列而无决策 |
| Result | 30s | 只有形容词、没有数字和下游效果 |

被追问时准备一句 **trade-off**（问题 2 的 1.2 ms）、一句 **XFN 边界**（问题 3 的 skill owner 是子团队）、一句 **系统性补丁**（问题 4 的 CI checklist）。
