# Book Outline — *Physical AI: From Sensors to Agents*

*A practitioner's guide to building AI systems that perceive, reason, and act in the physical world — under real hardware constraints.*

Status: Draft outline · 2026-10-11

---

## The thesis in one paragraph

Physical AI is AI that lives in hardware and interacts with the real world: it sees through cameras, hears through microphones, moves robot arms, drives vehicles, and runs on devices you hold or wear. Its defining challenge is not scale — it is *constraints*. Power budgets measured in milliwatts, latency budgets measured in milliseconds, memory budgets measured in megabytes, and silicon area budgets measured in square millimeters. Cloud AI asks "how many GPUs?" Physical AI asks "can this run at 30 Hz on 1.5 watts without burning the user's ear?" This book teaches engineers how to build, optimize, deploy, and debug AI systems that operate under those constraints — from the sensor pipeline to the robot actuator, from the NPU kernel to the edge-cloud routing decision.

## Why this book, why this author

Jeff Ren has spent years shipping machine learning on consumer devices under tight compute, memory, and power budgets. As a technical lead on a major smartphone camera team, he set a multi-generation roadmap that moved autofocus from heuristic algorithms to learned models, co-designed dedicated hardware that cut silicon area by 47%, and built an LLM-based triage agent grounded in domain physics. He has also prepared for and studied Staff-level AI engineering loops across frontier labs, hyperscalers, GPU vendors, and robotics startups. This book distills both the craft and the interview preparation into one resource.

## Target audience

| Reader | What they get |
|--------|---------------|
| ML/AI engineers moving into on-device, robotics, or edge roles | The systems and hardware knowledge these loops demand |
| Embedded/systems engineers pivoting to AI | A bridge from what they know (DMA, real-time, power budgets) to AI workloads |
| Robotics engineers who need the AI stack | VLA, sim-to-real, diffusion policy, and the inference runtime that supports them |
| Anyone interviewing for Physical AI roles | Patterns, model answers, and runnable code across the full stack |

## What makes it different

1. **Constraint-first.** Every chapter starts from a physical constraint (power, latency, area, bandwidth) and works toward the AI solution, not the other way around.
2. **Full vertical stack.** Sensor physics → ISP → NPU → model → agent → actuator, in one book.
3. **Runnable C++ and Python.** Physical AI interviews are C++ interviews. Code runs and is tested.
4. **From the practitioner who shipped it.** Not a survey of papers — a guide from someone who debugged analog-gain registers at 3 AM to save a product launch.

---

## Table of Contents

### Part I: The Physical AI Landscape (2 chapters)

**Chapter 1. What Is Physical AI?**
- The perception → reasoning → action loop
- Why physical AI is different from cloud AI: constraints are physics, not cost
- The industry map: smartphones, wearables, robots, autonomous vehicles, industrial IoT, smart cameras
- Role archetypes and which companies hire for what
- Where this book fits: the engineering side, not the research side

**Chapter 2. The Constraint Mindset**
- The five physical constraints: power, thermal, latency, memory, silicon area
- How to reason about them on a whiteboard: back-of-envelope calculations
- How interview loops test for constraint awareness (by level: Senior / Staff / Principal)
- The scoring rubric: what separates a weak answer from a Staff-level answer

### Part II: Sensing the Physical World (3 chapters)

**Chapter 3. The Perception Pipeline**
- From photons to tensors: sensor physics, Bayer patterns, ISP pipeline
- Camera 3A (auto-exposure, auto-white-balance, autofocus) as a real-time control system
- Multi-sensor systems: RGB + depth + IMU + LiDAR + tactile
- Sensor fusion fundamentals: temporal alignment, calibration, coordinate transforms
- *Worked problem:* Design a perception pipeline for a robot arm picking objects from a bin (RGB-D + force/torque)

**Chapter 4. Real-Time Data Paths**
- DMA, zero-copy, and `dma-buf`: moving sensor data without touching the CPU
- Ring buffers and producer-consumer patterns for frame pipelines
- ISP → NPU → CPU data flow on a mobile SoC
- Cache coherency between CPU and accelerators
- *Worked problem:* Design a frame pipeline that feeds camera frames to an on-device model at 30 fps with < 5 ms latency from sensor readout to inference start
- *Code:* Lock-free SPSC ring buffer (C++), dma-buf-style zero-copy pipeline

**Chapter 5. Concurrency for Physical AI**
- Why physical AI is a concurrency problem: multiple sensors, multiple consumers, hard deadlines
- Thread pools, lock-free queues, and bounded blocking queues
- The latest-value pattern: why you often want to drop frames, not queue them
- Deadlock discipline and lock ordering in multi-sensor systems
- Real-time scheduling: priority inversion, priority inheritance, RT patches
- *Code:* Thread-safe ring buffer, SPSC queue, bounded blocking queue (C++ and Python)

### Part III: Training Physical AI Models (3 chapters)

**Chapter 6. Pre-Training for Physical AI**
- Why Physical AI models need their own pre-training: the data is not text
- Vision foundation models: pre-training on images and video (DINOv2, SigLIP, VideoMAE)
- Multi-modal pre-training: aligning vision, language, and proprioception in a shared embedding space
- World models: learning physics from video — prediction, imagination, and planning (Genie, UniSim, DIAMOND)
- Data engines: building the flywheel of sensor data collection, curation, and labeling at scale
- Pre-training compute: distributed training across GPUs, compute-communication overlap, MFU optimization
- *Why this is different from LLM pre-training:* multi-modal inputs, action-conditioned prediction, physics grounding, orders of magnitude less data
- *Worked problem:* Design a pre-training pipeline for a vision-language model that will be fine-tuned into a VLA

**Chapter 7. Post-Training and Adaptation**
- Fine-tuning foundation models for physical tasks: from general vision to robot manipulation
- Imitation learning as post-training: behavior cloning, DAgger, and their failure modes
- Reinforcement learning on top of pre-trained representations: reward design is the hard part
- RLHF for robots: learning from human corrections and preferences on physical tasks
- Diffusion Policy training: denoising score matching for continuous action spaces
- Co-training across embodiments: Open X-Embodiment and cross-robot transfer
- Domain adaptation: adapting a model trained on one sensor/robot to another
- On-device fine-tuning and personalization: LoRA and adapter layers under memory constraints
- *Worked problem:* You have a pre-trained VLM and 50 hours of teleoperation data. Design the post-training pipeline to produce a manipulation policy.

**Chapter 8. Data, Simulation, and Evaluation**
- The data bottleneck: why robot data is scarce and expensive, and what to do about it
- Teleoperation pipelines: ALOHA, bimanual rigs, VR controllers — hardware and software
- Synthetic data from simulation: Isaac Sim, MuJoCo, procedural scene generation
- Data quality over quantity: what to collect, how to curate, when more data stops helping
- Evaluation for Physical AI: you can't A/B test a robot arm at scale
- Offline metrics vs real-world success rate: what correlates and what doesn't
- Benchmarks: LIBERO, RLBench, Meta-World, BridgeData — what each measures and what it hides
- *Worked problem:* You have 10 hours of demonstrations and budget for 50 more. Design the data collection strategy.

### Part IV: Inference Foundations (4 chapters)

**Chapter 9. LLM Anatomy for Physical AI Engineers**
- Transformer architecture: what you need to know, and what you can skip
- Attention, MLP, layer norm, embeddings — the compute and memory profile of each
- MoE: why it matters for edge (route to a smaller expert)
- Decoding: autoregressive generation, top-k, top-p, temperature
- *Why this is in a Physical AI book:* LLMs are now part of the robot stack (VLA), the wearable stack (voice assistants), and the IoT stack (edge agents)

**Chapter 10. Attention, KV Cache, and Memory**
- The KV cache: what it is, why it dominates memory, how to manage it
- FlashAttention: tiling to stay in SRAM
- PagedAttention: virtual memory for the KV cache
- Grouped-query attention and multi-query attention: trading compute for memory
- *Code:* KV cache implementation, FlashAttention from scratch, PagedAttention (Python + C++)

**Chapter 11. Kernels and GPU Programming**
- GEMM: the operation that dominates inference
- Memory hierarchy: registers → shared memory → L2 → HBM
- Tiling, coalescing, and occupancy
- Kernel fusion: why fused kernels win
- Profiling with nsight / rocprof
- *Code:* Tiled GEMM (C++), fused attention kernel sketch

**Chapter 12. ML Compilers and Graph Runtimes**
- What an ML compiler does: graph lowering, operator scheduling, memory planning
- TVM, XLA, TensorRT, ExecuTorch, ONNX Runtime, QNN
- Tensor lifetime analysis and memory planning for fixed-budget devices
- The compilation pipeline: ONNX → graph IR → schedule → codegen → binary
- *Worked problem:* A compiled model uses 1.8 GB peak memory. The device has 1.2 GB available. Reduce it without changing the model.

### Part V: AI on the Edge (4 chapters)

**Chapter 13. The NPU Stack**
- What an NPU is and why it exists: ops/watt vs GPU vs CPU
- The hardware: MAC arrays, activation buffers, weight SRAM, DMA engines
- The software stack: from ONNX/TFLite to compiler IR to hardware instructions
- Vendor stacks: Qualcomm QNN/HTP, Apple ANE, Google TPU (Pixel), MediaTek APU, Samsung NPU
- How to think about NPU programming without vendor docs
- *Worked problem:* A model runs 3x slower than expected on the NPU. Debug it.

**Chapter 14. Model Optimization for Devices**
- Quantization: PTQ, QAT, INT8, INT4, mixed precision
- Why quantization is not just "make the numbers smaller" — accuracy cliffs, per-channel vs per-tensor, calibration
- Pruning, distillation, and architecture search for edge
- Operator fusion and graph optimization
- Profiling: how to find the bottleneck (compute-bound vs memory-bound vs launch-bound)
- *Code:* INT8 quantization from scratch (Python + C++), operator fusion example

**Chapter 15. LLMs on Phones and Wearables**
- The memory wall: why a 7B model doesn't fit, and what you do about it
- KV cache management under fixed memory: block allocation, eviction, prefix sharing
- Speculative decoding on device: draft model on NPU, verify on cloud
- Power-aware inference: burst vs sustained, thermal throttling, battery-life SLA
- *Worked problem:* Run a 3B-parameter model on a phone with 6 GB RAM, targeting 30 tokens/s decode and < 500 ms TTFT

**Chapter 16. Hardware-Software Co-Design**
- The thesis: the best optimization is the one you never have to make at runtime
- Co-designing the algorithm and the silicon: the PDAF case study (47% area reduction)
- Moving compute from AP to dedicated hardware: when and why
- Fixed-function vs programmable: the trade-off space
- Working with hardware teams: the interface between algorithm and RTL
- *Worked problem:* You're given a 2 mm² budget for an on-chip accelerator. What do you put in it?

### Part VI: Embodied AI and Autonomous Systems (5 chapters)

**Chapter 17. Vision-Language-Action Models**
- What is a VLA? The architecture: vision encoder + language model + action head
- RT-2, OpenVLA, Octo, π₀: compare observation spaces, action spaces, data budgets
- Action tokenization and action chunking: why predict a trajectory, not a single step
- Diffusion Policy and ACT (Action Chunking with Transformers)
- The two-speed pattern: slow semantic planner + fast reactive controller
- *Worked problem:* Design a VLA loop running at 10–50 Hz. Walk through the latency budget.

**Chapter 18. Sim-to-Real and Robot Learning**
- Why simulation: data is the bottleneck, and real-robot hours are expensive
- GPU-parallel simulation: Isaac Gym/Lab, MuJoCo MJX, Genesis
- Domain randomization: visual, dynamics, contact — what to randomize and how much
- Teacher-student distillation: privileged information in sim, observation-only in real
- System identification and the residual sim-to-real gap
- *Worked problem:* A whole-body controller trained in simulation stumbles on real hardware. Debug the gap.

**Chapter 19. Robot Perception and Control**
- 3D spatial representations: point clouds, NeRF, 3D Gaussian Splatting, 6D pose
- SE(3), quaternions, rotation representations for learning
- Kinematics and dynamics: FK, IK, Jacobian, impedance control — the minimum a Physical AI engineer needs
- Joint-space vs end-effector actions: when to use which
- Force sensing, tactile sensing, and contact-rich manipulation
- Safety architecture: when a learned policy controls something that can hurt people

**Chapter 20. Autonomous Systems**
- Self-driving: perception → prediction → planning → control
- Sensor suites for autonomy: camera, LiDAR, radar, ultrasonic, HD maps
- BEV (Bird's Eye View) representations and transformer-based perception
- End-to-end vs modular: the trade-off
- Edge cases, long-tail scenarios, and the evaluation problem
- Drones, AGVs, and industrial robots: where the constraints differ
- *Worked problem:* Design the perception stack for a delivery robot operating in sidewalk environments

**Chapter 21. Automotive AI: From ADAS to the Software-Defined Vehicle**
- Why automotive is the highest-stakes Physical AI domain: safety-critical, real-time, mass-production
- The automotive AI SoC landscape: Qualcomm Snapdragon Ride / Digital Chassis, NVIDIA DRIVE, Mobileye EyeQ, Tesla FSD chip
- ADAS perception stack on an automotive SoC: camera, radar, LiDAR fusion on a power and thermal budget
- In-cabin AI: driver monitoring (DMS), occupant monitoring (OMS), gesture, gaze, and voice
- Digital cockpit: running multimodal LLMs for natural-language vehicle interaction
- Model deployment pipeline: from cloud training to automotive-grade NPU (QNN/TVM → safety-qualified runtime)
- Automotive safety standards: ISO 26262 (functional safety), SOTIF, ASIL levels — what they mean for AI
- OTA model updates: deploying new perception and cockpit models to vehicles in the field
- The software-defined vehicle thesis: why the car becomes a Physical AI platform
- *Worked problem:* Design the AI perception and cockpit stack for a next-generation vehicle on a Snapdragon Ride platform — partition workloads across CPU, GPU, NPU, and DSP under a 25W thermal budget
- *Worked problem:* A DMS model has a 0.1% false-negative rate for drowsiness detection. Is that safe enough? Walk through the safety analysis.

### Part VII: AI Agents in the Physical World (3 chapters)

**Chapter 22. Edge-Cloud Hybrid Routing**

- The core decision: what runs on-device, what goes to the cloud?
- Confidence-based cascading: local model scores → threshold → cloud escalation
- Network-aware routing: RTT estimation, offline fallback, graceful degradation
- Cost model: cloud API cost × call rate vs on-device power × battery life
- Fleet-scale routing: 100K cameras, each making independent routing decisions
- *Code:* Hybrid routing simulator (Python)

**Chapter 23. Always-On Cascaded Wake Systems**
- The power hierarchy: DSP (µW) → NPU (mW) → AP (W)
- Wake word detection → intent classification → full model activation
- Designing for milliwatt budgets: what can run always-on, what must be burst
- Voice assistants on embedded devices: AEC, VAD, barge-in, WebRTC
- Multi-modal always-on: audio + motion + visual change detection
- *Worked problem:* Design the always-on subsystem for smart glasses with a 2W total budget

**Chapter 24. Agents That Debug the Physical World**
- Why physical-world debugging is an AI problem: multi-modal logs, sensor dumps, simulation traces
- The architecture: ingestion → gatekeeping → tool-augmented reasoning → routing
- Knowledge graphs for hardware failure modes
- Modular skills: each sub-team owns a diagnostic skill
- From hallucination to grounded diagnosis: constraining inputs, not prompting harder
- *Worked problem:* Design a triage agent for a robotics fleet that processes sensor logs, runs simulations, and routes bugs to the right team

### Part VIII: The Interview Rounds (4 chapters)

**Chapter 25. Coding for Physical AI Roles**
- What's different: C++ is required, not optional
- Memory management, RAII, move semantics, smart pointers
- Data structures that matter: ring buffers, memory pools, aligned allocation, bitmaps
- Common patterns: producer-consumer, event dispatcher, state machines
- 15 worked problems with full C++ implementations

**Chapter 26. System Design for Physical AI**
- The model-aware design method: start from the model's compute/memory/latency profile
- The constraint checklist: power, thermal, latency, memory, bandwidth, silicon area
- Scaling drills adapted for physical AI: 10x cameras, 100x robots, 1000x sensors
- 8 fully worked designs:
  1. Smart glasses multimodal agent runtime
  2. 100K-camera hybrid routing system
  3. Real-time voice assistant on embedded hardware
  4. Robot fleet teleoperation data pipeline
  5. On-device LLM with cloud fallback
  6. Edge collaborative agent runtime (offline-capable)
  7. Perception pipeline for autonomous delivery robot
  8. AI-powered hardware debugging agent

**Chapter 27. AI-Enabled Coding Round**
- The format: unfamiliar codebase, AI assistant, time pressure
- The thesis: when code is cheap, the scarce skills are specifying and verifying
- The method: orient → hypothesize → delegate → verify
- 7 practice projects with failing tests (companion repo)
- What the interviewer watches for: judgment, not speed

**Chapter 28. Behavioral Stories for Physical AI Leaders**
- The theme library method: 6 themes, each with a STAR story and a trade-off
- Reframing hardware/embedded stories for AI leadership signals
- Staff-level delivery: technical depth, systemic impact, quantifiable results
- 6 worked examples covering: technical roadmap, HW/SW co-design innovation, XFN sensor-algorithm collaboration, data-driven disagreement resolution, debugging ambiguity in AI for hardware, engineering process and mentorship

### Appendices

**Appendix A. Numbers Every Physical AI Engineer Should Know**
- Latency numbers: DRAM access, PCIe transfer, NPU inference, cloud round-trip
- Power numbers: DSP, NPU, GPU, AP, radio
- Memory numbers: model sizes, KV cache sizes, activation memory
- Throughput numbers: tokens/s by model size and hardware

**Appendix B. ML Fundamentals Refresher**
- Backpropagation, gradient descent, regularization
- CNN, RNN/LSTM, Transformer — just enough to follow the chapters
- Evaluation metrics: precision, recall, mAP, success rate

**Appendix C. Companion Code Index**
- Every code listing in the book, with language, chapter, and repo path

---

## What's cut vs the original "From Kernels to Agents"

| Original content | Decision | Rationale |
|-----------------|----------|-----------|
| RAG system design (Ch 22) | Cut | Pure cloud pattern, not Physical AI |
| Rate limiting (Ch 21) | Cut | Cloud API concern |
| Streaming chat backend (Ch 20) | Cut | Cloud product pattern |
| Code sandbox (Ch 25) | Cut | Cloud product pattern |
| LLM evaluation (Ch 24) | Cut | Folded into Appendix B and Chapter 15 (sim-to-real eval) |
| Pre-training (Ch 6) | **Reframed** | Rewritten for Physical AI: vision foundation models, world models, multi-modal pre-training — not LLM text pre-training |
| Post-training (Ch 7) | **Reframed** | Rewritten for Physical AI: imitation learning, RL fine-tuning, diffusion policy training, cross-embodiment co-training |
| Serving at scale (Ch 12) | Reduced | Cloud serving partially kept in edge-cloud routing context |
| Accelerator data planes (Ch 16) | Kept | Relevant to NPU and co-design |
| Compute-communication overlap (Ch 18) | Reduced | Folded into GPU/NPU chapters |
| Camera/sensor content | **Added back** | Core to Physical AI — excluded in original due to scope |
| Embodied AI (Ch 27) | **Expanded to 4 chapters** | Central to Physical AI |
| Wearables/edge (Ch 15) | **Expanded** | Central to Physical AI |
| HW/SW co-design | **New chapter** | Jeff's strongest unique angle |
| Autonomous systems | **New chapter** | Major Physical AI domain |

## Estimated scope

~28 chapters + 3 appendices · ~140,000 words (~470 pages) · ~125 figures

---

## Source mapping

| Chapter | Primary sources from this repo |
|---------|-------------------------------|
| Ch 3 (Perception) | `camera/3A.md`, `camera/sensor.md`, `camera/camera_driver.md` |
| Ch 4 (Data paths) | `concurrency/spsc_ring_buffer.*`, `company/openai/embedded-sensor-event-loop.md` |
| Ch 5 (Concurrency) | `concurrency/CHAPTER_CONCURRENCY.md`, `concurrency/*.cpp` |
| Ch 6 (Pre-training) | `LLM/预训练.md`, `LLM/LLM训练计算通信重叠与MFU优化.md`; new writing on vision/world models |
| Ch 7 (Post-training) | `LLM/后训练.md`, `embodied_AI/embodied_ai_interview_guide.md` (imitation learning, RL sections) |
| Ch 8 (Data/Sim/Eval) | `embodied_AI/embodied_ai_interview_guide.md`, `embodied_AI/ai_data_annotation_engineering_guide.md` |
| Ch 9 (LLM anatomy) | `LLM/注意力实现.md`, `LLM/MoE.md`, `LLM/softmax.md` |
| Ch 10 (KV/Flash/Paged) | `LLM/kv_cache.*`, `LLM/flash_attention.*`, `LLM/paged_attention.*` |
| Ch 11 (Kernels) | `LLM/矩阵乘法.md`, `LLM/LLM算子.md`, `company/microsoft/01-gemm-*` |
| Ch 12 (ML compilers) | `company/amazon/23-Neuron*`, `LLM/算子融合.md`, `LLM/tensor_lifetime.md` |
| Ch 13 (NPU) | `AI_edge/16-*`, `AI_edge/qualcomm_senior_staff_*` |
| Ch 14 (Quantization) | `LLM/量化计算.md`, `AI_edge/27-*` |
| Ch 15 (LLMs on phone) | `AI_edge/27-*`, `company/openai/smart-glasses-ai-runtime.md` |
| Ch 16 (Co-design) | `behavior/meta-staff-core-themes.md` (stories 1–2), camera HW content |
| Ch 17 (VLA) | `embodied_AI/embodied_ai_interview_guide.md`, `embodied_AI/18-*` |
| Ch 18 (Sim-to-real) | `embodied_AI/embodied_ai_interview_guide.md` |
| Ch 19 (Robot perception) | `embodied_AI/embodied_ai_interview_guide.md` |
| Ch 20 (Autonomous) | `company/nvidia/03-AV*`, `company/nvidia/01-系统设计.md` |
| Ch 21 (Automotive AI) | `AI_edge/qualcomm_senior_staff_*`, `AI_edge/16-*`; author's Qualcomm automotive AI experience |
| Ch 22 (Hybrid routing) | `company/openai/hybrid-sensor-routing.md` |
| Ch 23 (Always-on) | `company/openai/smart-glasses-ai-runtime.md`, `company/openai/realtime-voice-assistant.md` |
| Ch 24 (Debug agents) | `behavior/meta-staff-star.md` (AF triage agent), `AI_native_coding/` |
| Ch 25 (Coding) | `c++/*`, `leetcode/*`, `company/amazon/examples/*` |
| Ch 26 (System design) | `company/openai/`, `company/nvidia/01-*`, `company/amazon/17-*` |
| Ch 27 (AI-enabled coding) | `AI_native_coding/chapter_ai_native_coding.md`, `AI_native_coding/playbook.md` |
| Ch 28 (Behavioral) | `behavior/meta-staff-*.md` (reframed for Physical AI) |
