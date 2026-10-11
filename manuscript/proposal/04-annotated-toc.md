# Annotated Table of Contents — *Physical AI: From Sensors to Agents*

*Building AI Systems That Perceive, Reason, and Act in the Physical World*

8 parts · 28 chapters · 3 appendices · ~140,000 words (~470 printed pages) · ~125 original figures

Sample chapters are marked ★.

Front matter: a preface on who the book is for and how to use it by role archetype, and a note on the companion code repository.

## Part I — The Physical AI Landscape

*What Physical AI is, why it is different from cloud AI, and how to think about the physical constraints that define it.*

### Chapter 1. What Is Physical AI?

Physical AI is AI that perceives, reasons, and acts in the physical world. This chapter defines the perception → reasoning → action loop and explains why Physical AI is fundamentally different from cloud AI: constraints are physics, not cost. It maps the industry landscape — smartphones, wearables, robots, autonomous vehicles, industrial IoT, smart cameras — identifies the role archetypes and which companies hire for each, and explains where this book fits: the engineering side, not the research side.

Questions the chapter prepares the reader to answer: *What is Physical AI, and how does it differ from cloud AI?* · *Which Physical AI role fits my background?* · *Which chapters should I prioritize?*

~3,500 words · 3 figures

### Chapter 2. The Constraint Mindset

The five physical constraints — power, thermal, latency, memory, silicon area — and how to reason about them on a whiteboard with back-of-envelope calculations. How interview loops test for constraint awareness by level (Senior, Staff, Principal), and the scoring rubric that separates a weak answer from a Staff-level answer. Closes with study plans keyed to the role archetypes from Chapter 1.

Questions the chapter prepares the reader to answer: *What separates a Staff answer from a Senior one on a constraint question?* · *How should I split limited prep time?*

~4,000 words · 4 figures

---

## Part II — Sensing the Physical World

*How data moves from sensors to models: the perception pipeline, real-time data paths, and the concurrency patterns that keep them running at frame rate.*

### Chapter 3. The Perception Pipeline

From photons to tensors: sensor physics, Bayer patterns, and the ISP pipeline. Camera 3A (auto-exposure, auto-white-balance, autofocus) as a real-time control system. Multi-sensor systems (RGB + depth + IMU + LiDAR + tactile) and sensor fusion fundamentals: temporal alignment, calibration, and coordinate transforms.

Questions the chapter prepares the reader to answer: *Walk through the camera ISP pipeline.* · *How do you temporally align data from sensors running at different frame rates?* · *Design a perception pipeline for a robot arm picking objects from a bin.*

~5,000 words · 5 figures

### Chapter 4. Real-Time Data Paths

DMA, zero-copy, and dma-buf: moving sensor data without touching the CPU. Ring buffers and producer-consumer patterns for frame pipelines. ISP → NPU → CPU data flow on a mobile SoC. Cache coherency between CPU and accelerators.

Questions the chapter prepares the reader to answer: *Explain dma-buf and why it matters for on-device inference.* · *Design a frame pipeline at 30 fps with < 5 ms sensor-to-inference latency.*

~4,500 words · 5 figures · Code: lock-free SPSC ring buffer (C++), zero-copy pipeline

### Chapter 5. Concurrency for Physical AI

Why Physical AI is a concurrency problem: multiple sensors, multiple consumers, hard deadlines. Thread pools, lock-free queues, and bounded blocking queues. The latest-value pattern (drop frames, don't queue them). Deadlock discipline and lock ordering in multi-sensor systems. Real-time scheduling: priority inversion, priority inheritance, RT patches.

Questions the chapter prepares the reader to answer: *Implement a lock-free SPSC ring buffer.* · *When should you drop data instead of queuing it?* · *Explain priority inversion.*

~6,000 words · 5 figures · Code: thread-safe ring buffer, SPSC queue, bounded blocking queue (C++ and Python)

---

## Part III — Training Physical AI Models

*How Physical AI models are trained: vision foundation models, world models, imitation learning, diffusion policies, and the data and simulation infrastructure underneath.*

### Chapter 6. Pre-Training for Physical AI

Why Physical AI models need their own pre-training — the data is not text. Vision foundation models (DINOv2, SigLIP, VideoMAE). Multi-modal pre-training: aligning vision, language, and proprioception in a shared embedding space. World models: learning physics from video (Genie, UniSim, DIAMOND). Data engines. Pre-training compute: distributed training, compute-communication overlap, MFU optimization. Why this is different from LLM pre-training.

Questions the chapter prepares the reader to answer: *How is pre-training a vision foundation model different from pre-training an LLM?* · *What is a world model?* · *Design a pre-training pipeline for a VLM that will become a VLA.*

~5,000 words · 5 figures

### Chapter 7. Post-Training and Adaptation

Fine-tuning foundation models for physical tasks. Imitation learning as post-training: behavior cloning, DAgger, failure modes. RL on top of pre-trained representations: reward design is the hard part. RLHF for robots. Diffusion Policy training: denoising score matching for continuous action spaces. Co-training across embodiments (Open X-Embodiment). Domain adaptation. On-device fine-tuning with LoRA under memory constraints.

Questions the chapter prepares the reader to answer: *What is behavior cloning's failure mode on a real robot?* · *Design the post-training pipeline from a pre-trained VLM and 50 hours of teleoperation data to a manipulation policy.* · *How does Diffusion Policy training work?*

~5,000 words · 5 figures

### Chapter 8. Data, Simulation, and Evaluation

The data bottleneck: why robot data is scarce and expensive. Teleoperation pipelines (ALOHA, bimanual rigs, VR controllers). Synthetic data from simulation (Isaac Sim, MuJoCo, procedural scenes). Data quality over quantity. Evaluation for Physical AI: offline metrics vs real-world success rate. Benchmarks (LIBERO, RLBench, Meta-World, BridgeData).

Questions the chapter prepares the reader to answer: *Why is robot data the bottleneck?* · *Design a teleoperation data pipeline.* · *You have 10 hours of demonstrations and budget for 50 more — design the collection strategy.* · *Policy A succeeds 70/100, Policy B 76/100 — is B genuinely better?*

~5,000 words · 5 figures

---

## Part IV — Inference Foundations

*The LLM inference stack that Physical AI now depends on: transformer anatomy, the KV cache, GPU kernels, and ML compilers.*

### Chapter 9. LLM Anatomy for Physical AI Engineers

A decoder-only transformer walked through operator by operator: embeddings, layer norm, attention (MHA, GQA), RoPE, SwiGLU, and the LM head. For each operator: FLOPs, memory traffic, and numerical pitfalls. MoE: why it matters for edge. Decoding: autoregressive generation, top-k, top-p, temperature. Why LLMs are now part of the Physical AI stack (VLA, voice assistants, edge agents).

Questions the chapter prepares the reader to answer: *Why did GQA replace MHA?* · *Where do the FLOPs go in one forward pass?* · *Why subtract the max in softmax?*

~5,000 words · 5 figures · Code: softmax / GELU / RoPE (C++)

### Chapter 10. Attention, KV Cache, and Memory ★

The KV cache: what it is, why it dominates memory, how to manage it. FlashAttention: tiling to stay in SRAM. PagedAttention: virtual memory for the KV cache. GQA and MQA: trading compute for memory. Shows the book's core method: derive, implement, test, then practice the spoken answer.

Questions the chapter prepares the reader to answer: *Why does the KV cache dominate decode memory?* · *Explain FlashAttention — why is it faster despite more FLOPs?* · *What problem does PagedAttention solve?*

~7,500 words · 7 figures · Code: KV cache, FlashAttention, PagedAttention (Python + C++)

### Chapter 11. Kernels and GPU Programming

GEMM: the operation that dominates inference. Memory hierarchy: registers → shared memory → L2 → HBM. Tiling, coalescing, and occupancy. Kernel fusion. Profiling with nsight / rocprof.

Questions the chapter prepares the reader to answer: *Why is GEMM the bottleneck?* · *When is a kernel compute-bound vs memory-bound?* · *Why does fusing two kernels beat running them separately?*

~5,000 words · 5 figures · Code: tiled GEMM (C++), fused attention kernel sketch

### Chapter 12. ML Compilers and Graph Runtimes

What an ML compiler does: graph lowering, operator scheduling, memory planning. TVM, XLA, TensorRT, ExecuTorch, ONNX Runtime, QNN. Tensor lifetime analysis and memory planning for fixed-budget devices.

Questions the chapter prepares the reader to answer: *Walk through the compilation pipeline from ONNX to device binary.* · *A compiled model uses 1.8 GB peak memory; the device has 1.2 GB. Reduce it without changing the model.*

~4,500 words · 4 figures

---

## Part V — AI on the Edge

*Deploying AI on devices with fixed power, memory, and thermal budgets: the NPU, quantization, on-device LLMs, and the ultimate optimization — co-designing the algorithm and the silicon.*

### Chapter 13. The NPU Stack

What an NPU is and why it exists: ops/watt vs GPU vs CPU. The hardware: MAC arrays, activation buffers, weight SRAM, DMA engines. The software stack: from ONNX/TFLite to compiler IR to hardware instructions. Vendor stacks (Qualcomm QNN/HTP, Apple ANE, Google TPU, MediaTek APU, Samsung NPU).

Questions the chapter prepares the reader to answer: *Why does an NPU exist alongside GPU and CPU?* · *Explain the NPU software stack.* · *A model runs 3x slower than expected on the NPU — debug it.*

~4,500 words · 5 figures

### Chapter 14. Model Optimization for Devices

Quantization: PTQ, QAT, INT8, INT4, mixed precision. Accuracy cliffs, per-channel vs per-tensor, calibration. Pruning, distillation, and architecture search for edge. Operator fusion and graph optimization. Profiling: compute-bound vs memory-bound vs launch-bound.

Questions the chapter prepares the reader to answer: *PTQ vs QAT — when does PTQ fail?* · *Per-channel vs per-tensor quantization — why does per-channel matter?* · *A model quantized to INT8 drops 5% accuracy on one class — diagnose it.*

~4,500 words · 4 figures · Code: INT8 quantization from scratch (Python + C++), operator fusion example

### Chapter 15. LLMs on Phones and Wearables

The memory wall. KV cache management under fixed memory: block allocation, eviction, prefix sharing. Speculative decoding on device. Power-aware inference: burst vs sustained, thermal throttling, battery-life SLA.

Questions the chapter prepares the reader to answer: *Run a 3B model on a phone with 6 GB RAM — 30 tok/s decode, < 500 ms TTFT.* · *How do you manage the KV cache when device memory is fixed?* · *Explain speculative decoding for on-device use.*

~4,500 words · 4 figures

### Chapter 16. Hardware-Software Co-Design ★

The thesis: the best optimization is the one you never have to make at runtime. Co-designing the algorithm and the silicon: the PDAF case study (47% area reduction). Moving compute from AP to dedicated hardware. Fixed-function vs programmable. Working with hardware teams: the interface between algorithm and RTL.

Questions the chapter prepares the reader to answer: *When should compute move from the AP to dedicated hardware?* · *You're given a 2 mm² budget for an on-chip accelerator — what do you put in it?* · *Fixed-function vs programmable — trade-offs?*

~4,500 words · 4 figures

---

## Part VI — Embodied AI and Autonomous Systems

*AI that moves things: VLA models, sim-to-real transfer, robot perception and control, autonomous systems, and the automotive AI platform.*

### Chapter 17. Vision-Language-Action Models

What is a VLA? Vision encoder + language model + action head. RT-2, OpenVLA, Octo, π₀. Action tokenization and chunking. Diffusion Policy and ACT. The two-speed pattern: slow semantic planner + fast reactive controller.

Questions the chapter prepares the reader to answer: *Explain a VLA end to end: input, output, control frequency.* · *Compare RT-2, OpenVLA, π₀, and Diffusion Policy.* · *Design a VLA loop at 10–50 Hz — walk through the latency budget.*

~5,000 words · 5 figures

### Chapter 18. Sim-to-Real and Robot Learning

Why simulation: data is the bottleneck. GPU-parallel simulation (Isaac Gym/Lab, MuJoCo MJX, Genesis). Domain randomization: visual, dynamics, contact. Teacher-student distillation with privileged information. System identification and the residual gap.

Questions the chapter prepares the reader to answer: *How do you close the sim-to-real gap?* · *A whole-body controller trained in sim stumbles on real hardware — debug it.* · *Which domain gaps matter most?*

~4,500 words · 5 figures

### Chapter 19. Robot Perception and Control

3D representations: point clouds, NeRF, 3DGS, 6D pose. SE(3), quaternions, rotation representations for learning. Kinematics and dynamics: FK, IK, Jacobian, impedance control. Joint-space vs end-effector actions. Force/tactile sensing. Safety architecture for learned policies near people.

Questions the chapter prepares the reader to answer: *Design a perception pipeline for 6D pose estimation.* · *Joint-space vs Cartesian actions — when?* · *Design safety for a learned whole-body policy near people.*

~5,000 words · 5 figures

### Chapter 20. Autonomous Systems

Self-driving: perception → prediction → planning → control. Sensor suites for autonomy. BEV representations and transformer-based perception. End-to-end vs modular. Edge cases and the evaluation problem. Drones, AGVs, industrial robots.

Questions the chapter prepares the reader to answer: *Walk through the AV stack from sensor to actuator.* · *End-to-end vs modular — trade-offs?* · *Design the perception stack for a delivery robot on sidewalks.*

~4,500 words · 5 figures

### Chapter 21. Automotive AI: From ADAS to the Software-Defined Vehicle

Why automotive is the highest-stakes Physical AI domain: safety-critical, real-time, mass-production. The automotive AI SoC landscape (Qualcomm Snapdragon Ride / Digital Chassis, NVIDIA DRIVE, Mobileye EyeQ, Tesla FSD chip). ADAS perception on an automotive SoC: camera, radar, LiDAR fusion under power and thermal budgets. In-cabin AI: driver monitoring (DMS), occupant monitoring (OMS), gesture, gaze, and voice. Digital cockpit: running multimodal LLMs for natural-language vehicle interaction. Model deployment pipeline from cloud training to automotive-grade NPU. Automotive safety standards (ISO 26262, SOTIF, ASIL levels) and what they mean for AI. OTA model updates. The software-defined vehicle thesis.

Questions the chapter prepares the reader to answer: *Design the AI perception and cockpit stack for a next-gen vehicle — partition workloads across CPU, GPU, NPU, and DSP under a 25W thermal budget.* · *What is ISO 26262 and ASIL, and what do they mean for deploying learned models in a vehicle?* · *A DMS model has a 0.1% false-negative rate for drowsiness detection — is that safe enough?* · *Compare Qualcomm Snapdragon Ride, NVIDIA DRIVE, and Mobileye EyeQ.*

~5,000 words · 5 figures

---

## Part VII — AI Agents in the Physical World

*AI agents that make decisions about physical things: routing inference between device and cloud, waking up at the right time, and debugging hardware failures.*

### Chapter 22. Edge-Cloud Hybrid Routing

The core decision: device or cloud? Confidence-based cascading. Network-aware routing: RTT estimation, offline fallback, graceful degradation. Cost model. Fleet-scale routing for 100K cameras.

Questions the chapter prepares the reader to answer: *When should inference run on-device vs cloud?* · *Design a confidence cascade for a 100K-camera fleet.* · *How do you handle intermittent connectivity?*

~4,500 words · 4 figures · Code: hybrid routing simulator (Python)

### Chapter 23. Always-On Cascaded Wake Systems

The power hierarchy: DSP (µW) → NPU (mW) → AP (W). Wake word → intent → full model. Milliwatt budgets. Voice assistants: AEC, VAD, barge-in, WebRTC. Multi-modal always-on.

Questions the chapter prepares the reader to answer: *Design the always-on subsystem for smart glasses at 2W total.* · *Explain the cascaded wake hierarchy.* · *How does barge-in work?*

~4,500 words · 4 figures

### Chapter 24. Agents That Debug the Physical World

Physical-world debugging as an AI problem: multi-modal logs, sensor dumps, simulation traces. Ingestion → gatekeeping → tool-augmented reasoning → routing. Knowledge graphs for failure modes. Modular skills. From hallucination to grounded diagnosis.

Questions the chapter prepares the reader to answer: *Design a triage agent for a robotics fleet.* · *How do you ground an LLM agent in domain-specific physics?* · *How do you handle hallucination when wrong answers cause hardware damage?*

~4,500 words · 4 figures

---

## Part VIII — The Interview Rounds

*The four types of round Physical AI candidates face: coding, system design, AI-enabled coding, and behavioral.*

### Chapter 25. Coding for Physical AI Roles

C++ is required, not optional. Memory management, RAII, move semantics, smart pointers. Data structures: ring buffers, memory pools, aligned allocation, bitmaps. Patterns: producer-consumer, event dispatcher, state machines. 15 worked problems with full C++ implementations.

~6,000 words · 3 figures · Code: 15 C++ implementations

### Chapter 26. System Design for Physical AI

The model-aware design method with the Physical AI constraint checklist. Scaling drills for Physical AI (10x cameras, 100x robots, 1000x sensors). Eight fully worked designs: smart glasses runtime, 100K-camera routing, voice assistant, robot fleet teleoperation, on-device LLM, edge collaborative agent, delivery robot perception, hardware debugging agent.

~7,500 words · 8 figures

### Chapter 27. The AI-Enabled Coding Round ★

The format: unfamiliar codebase, AI assistant, time pressure. The thesis: when code is cheap, the scarce skills are specifying and verifying. The method: orient → hypothesize → delegate → verify. Seven practice projects with failing tests. What the interviewer watches for: judgment, not speed.

~7,500 words · 5 figures · Code: 7 practice projects in companion repo

### Chapter 28. Behavioral Stories for Physical AI Leaders

The theme library method: 6 themes, each with a STAR story and a trade-off. Reframing hardware/embedded stories for AI leadership signals. Staff-level delivery. Six worked examples: technical roadmap, HW/SW co-design innovation, XFN collaboration, disagreement resolution, debugging ambiguity, engineering process and mentorship.

~5,000 words · 2 figures

---

## Appendices

### Appendix A. Numbers Every Physical AI Engineer Should Know

Latency (DRAM, PCIe, NPU inference, cloud round-trip), power (DSP, NPU, GPU, AP, radio), memory (model sizes, KV cache, activation memory), throughput (tokens/s by model and hardware). One sheet for the night before.

### Appendix B. ML Fundamentals Refresher

Backpropagation, gradient descent, regularization. CNN, RNN/LSTM, Transformer. Evaluation metrics. For systems engineers who have not trained a model in years.

### Appendix C. Companion Code Index

Every runnable file referenced in the book, with language, chapter, and the command that runs it.
