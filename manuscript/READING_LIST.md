# Physical AI Reading List

Representative papers organized by book part. Start with the ★ papers in each section — they are the most influential or the best entry point.

Last updated: 2026-10-11

---

## Part II — Sensing the Physical World

### Perception and Sensor Fusion

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ DINOv2: Learning Robust Visual Features without Supervision (Oquab et al.) | TMLR 2024 (arXiv 2304.07193, Apr 2023) | The dominant self-supervised vision backbone. 86.5% ImageNet linear-probe with ViT-g. Used as the vision encoder in OpenVLA and many VLA models |
| SigLIP: Sigmoid Loss for Language Image Pre-Training (Zhai et al.) | ICCV 2023 (arXiv 2303.15343) | Replaces CLIP's softmax contrastive loss with a pairwise sigmoid, enabling efficient batch-size scaling. Used alongside DINOv2 in OpenVLA |
| DINOv2 Meets Text: A Unified Framework for Image- and Pixel-Level Vision-Language Alignment (Li et al.) | arXiv 2412.16334, Dec 2024 | Adds vision-language alignment to DINOv2, bridging self-supervised and text-supervised representations |

---

## Part III — Training Physical AI Models

### Vision Foundation Models and World Models

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ Genie: Generative Interactive Environments (Bruce et al., Google DeepMind) | ICML 2024 (arXiv 2402.15391) | Learns playable 2D environments from unlabeled internet video. Introduced action-conditioned world models at scale |
| Genie 2: A Large-Scale Foundation World Model (Google DeepMind) | Technical report, Dec 2024 | Extends to 3D scene generation from a single image. Limited to ~20s coherent sequences; not real-time |
| UniSim: Learning Interactive Real-World Simulators (Yang et al., Google) | ICLR 2024 (arXiv 2310.06114) | Trains a universal simulator on mixed data (internet video, robotics, synthetic 3D). Accepts text, robot actions, or camera poses as conditioning |
| DIAMOND: Diffusion for World Modeling (Alonso et al.) | NeurIPS 2024 (arXiv 2405.12399) | Studies how visual fidelity in world models affects downstream RL control quality |
| VideoMAE V2: Scaling Video Masked Autoencoders (Wang et al.) | CVPR 2023 (arXiv 2303.16727) | Video self-supervised pre-training at scale. Relevant for temporal representations in Physical AI |

### Imitation Learning, RL, and Diffusion Policies

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ Diffusion Policy: Visuomotor Policy Learning via Action Diffusion (Chi et al.) | RSS 2023 (arXiv 2303.04137) | Denoising diffusion for continuous robot actions. The paper that made diffusion-based policies practical for manipulation |
| Learning Fine-Grained Bimanual Manipulation with Low-Cost Hardware — ALOHA (Zhao et al.) | RSS 2023 (arXiv 2304.13705) | Low-cost bimanual teleoperation rig + Action Chunking with Transformers (ACT). Opened up affordable robot data collection |
| Mobile ALOHA (Fu et al.) | arXiv 2401.02117, Jan 2024 | Extends ALOHA to mobile manipulation with co-training on diverse data |
| Open X-Embodiment: Robotic Learning Datasets and RT-X Models (Open X-Embodiment Collaboration) | ICRA 2024 (arXiv 2310.08864) | 1M+ episodes across 22 embodiments from 21 institutions. The largest multi-embodiment robot dataset |

### Data, Simulation, and Evaluation

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ LIBERO: Benchmarking Knowledge Transfer for Lifelong Robot Learning (Liu et al.) | NeurIPS 2023 | 130 manipulation tasks, 4 task suites testing spatial, object, goal, and long-horizon knowledge transfer |
| RLBench: The Robot Learning Benchmark (James et al.) | IEEE RA-L 2020 | 100 tasks in simulation with dense reward and language descriptions. Still widely used for VLA evaluation |

---

## Part IV — Inference Foundations

### Attention and KV Cache

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ FlashAttention: Fast and Memory-Efficient Exact Attention with IO-Awareness (Dao et al.) | NeurIPS 2022 (arXiv 2205.14135) | Tiling attention to stay in SRAM. The foundational IO-aware algorithm |
| FlashAttention-2: Faster Attention with Better Parallelism and Work Partitioning (Dao) | ICLR 2024 (arXiv 2307.08691) | ~2x over FlashAttention via better thread-block work partitioning |
| FlashAttention-3: Fast and Accurate Attention with Asynchrony and Low-precision (Dao et al.) | arXiv 2407.08608, Jul 2024 | Hopper GPU (H100) specific: fp8, warp-specialized pipelining. Shows how attention kernels co-evolve with hardware |
| ★ Efficient Memory Management for Large Language Model Serving with PagedAttention (Kwon et al.) | SOSP 2023 (arXiv 2309.06180) | Virtual-memory-inspired KV cache management. The paper behind vLLM |
| GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints (Ainslie et al., Google) | EMNLP 2023 (arXiv 2305.13245) | Grouped-query attention: the middle ground between MHA and MQA. Used in Llama 2/3, Gemma, Mistral |

### GPU Kernels and ML Compilers

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ TVM: An Automated End-to-End Optimizing Compiler for Deep Learning (Chen et al.) | OSDI 2018 | The foundational ML compiler. Still the basis for edge deployment stacks (Apache TVM, Qualcomm AI Engine Direct) |
| Triton: An Intermediate Language and Compiler for Tiled Neural Network Computations (Tillet et al.) | MLSys 2019 | Write GPU kernels in Python. Increasingly used for custom attention/GEMM kernels |
| ExecuTorch (Meta) | Open-source, 2024 | PyTorch-native on-device inference. Delegates to CoreML, QNN, XNNPACK. The paper-equivalent is Meta's tech blog |

---

## Part V — AI on the Edge

### Quantization and Model Compression

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ AWQ: Activation-aware Weight Quantization for On-Device LLM Compression and Acceleration (Lin et al., MIT) | MLSys 2024 Best Paper (arXiv 2306.00978) | Identifies salient weight channels from activation statistics. 3x+ speedup on mobile GPU via TinyChat. The best current INT4 PTQ method |
| GPTQ: Accurate Post-Training Quantization for Generative Pre-Trained Transformers (Frantar et al.) | ICLR 2023 (arXiv 2210.17323) | Layer-wise quantization using approximate second-order information. The first practical 3-4 bit LLM quantization |
| QLoRA: Efficient Finetuning of Quantized LLMs (Dettmers et al.) | NeurIPS 2023 (arXiv 2305.14314) | 4-bit NormalFloat + double quantization + paged optimizers. Fine-tune 65B models on a single 48GB GPU |
| SmoothQuant: Accurate and Efficient Post-Training Quantization for Large Language Models (Xiao et al.) | ICML 2023 (arXiv 2211.10438) | Migrates quantization difficulty from activations to weights via per-channel scaling. W8A8 without accuracy loss |

### Speculative Decoding

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ Fast Inference from Transformers via Speculative Decoding (Leviathan et al., Google) | ICML 2023 (arXiv 2211.17192) | Draft-then-verify with mathematical guarantees on output distribution preservation |
| Accelerating Large Language Model Decoding with Speculative Sampling (Chen et al., DeepMind) | arXiv 2302.01318, Feb 2023 | Independent parallel discovery of speculative decoding |
| Medusa: Simple LLM Inference Acceleration Framework with Multiple Decoding Heads (Cai et al.) | ICML 2024 (arXiv 2401.10774) | No draft model — adds lightweight heads to the target model itself. 2.2–3.6x speedup |
| EAGLE: Speculative Sampling Requires Rethinking Feature Uncertainty (Li et al.) | ICML 2024 (arXiv 2401.15077) | Drafts at the feature level (second-to-top layer). 2.7–3.5x on Llama 2 Chat 70B. EAGLE-2 adds dynamic draft trees |

---

## Part VI — Embodied AI and Autonomous Systems

### Vision-Language-Action Models

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ RT-2: Vision-Language-Action Models Transfer Web Knowledge to Robotic Control (Brohan et al., Google DeepMind) | JMLR 2023 (arXiv 2307.15818) | The paper that proved web-scale VLM knowledge transfers to robot control. Multi-stage semantic reasoning (e.g., pick the improvised hammer) |
| ★ π₀: A Vision-Language-Action Flow Model for General Robot Control (Black et al., Physical Intelligence) | arXiv 2410.24164, Oct 2024 | 3B-parameter VLA using flow matching for continuous actions at up to 50 Hz. Demonstrated across 7 embodiments including bimanual ALOHA |
| OpenVLA: An Open-Source Vision-Language-Action Model (Kim et al.) | arXiv 2406.09246, Jun 2024 | Open-source 7B VLA fusing DINOv2 + SigLIP with Llama 2. Outperforms RT-2-X on 29 tasks with 7x fewer parameters |
| Octo: An Open-Source Generalist Robot Policy (Team et al., UC Berkeley) | RSS 2024 (arXiv 2405.12213) | Lightweight open-source policy (27M–93M params) trained on Open X-Embodiment. Diffusion action head |
| RT-1: Robotics Transformer for Real-World Control at Scale (Brohan et al., Google) | RSS 2023 (arXiv 2212.06817) | The predecessor: tokenized actions, 130K real episodes. Established the VLA paradigm |

### Sim-to-Real Transfer

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ Isaac Lab: A Unified Framework for Robot Learning (Mittal et al., NVIDIA / ETH Zurich) | arXiv 2511.04831, 2025 | GPU-accelerated simulation framework for multi-modal robot learning. Covers domain randomization for physics and rendering |
| Understanding Domain Randomization for Sim-to-Real Transfer (Chen et al.) | ICLR 2022 | Theoretical bounds on the sim-to-real gap. Argues that history-dependent policies matter under randomization |
| Learning Agile and Dynamic Motor Skills for Legged Robots (Hwangbo et al., ETH Zurich) | Science Robotics, 2019 | Teacher-student distillation with privileged information in simulation. The template many modern locomotion papers follow |
| Sim-to-Real: Learning Agile Locomotion For Quadruped Robots (Tan et al., Google Brain) | RSS 2018 | Domain randomization + system identification for quadruped transfer. Still the go-to reference |

### Robot Perception and 3D

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ 3D Gaussian Splatting for Real-Time Radiance Field Rendering (Kerbl et al.) | SIGGRAPH 2023 | Real-time novel view synthesis. Increasingly used for robot scene understanding and manipulation |
| NeRF: Representing Scenes as Neural Radiance Fields for View Synthesis (Mildenhall et al.) | ECCV 2020 (CACM 2022) | The foundational neural 3D representation. Read before Gaussian Splatting |
| FoundationPose: Unified 6D Pose Estimation and Tracking of Novel Objects (Wen et al., NVIDIA) | CVPR 2024 (arXiv 2312.08344) | 6D pose estimation without object-specific training. Relevant for robot grasping |
| On the Continuity of Rotation Representations in Neural Networks (Zhou et al.) | CVPR 2019 | Why quaternions are a bad regression target and 6D/9D rotation representations work better. Short and essential |

### Autonomous Driving

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ UniAD: Planning-oriented Autonomous Driving (Hu et al.) | CVPR 2023 (Best Paper) | Unifies perception, prediction, and planning in one transformer. Vision-only, end-to-end |
| BEVFormer: Learning Bird's-Eye-View Representation from Multi-Camera Images via Spatiotemporal Transformers (Li et al.) | ECCV 2022, extended in TPAMI | The standard BEV transformer. 74.1 NDS on nuScenes test. Used as the perception backbone in UniAD |
| VAD: Vectorized Scene Representation for Efficient Autonomous Driving (Jiang et al.) | ICCV 2023 | Vectorized representation removes HD-map dependency. Simpler and faster than UniAD |
| End-to-End Autonomous Driving: Challenges and Frontiers (Chen et al.) | TPAMI 2024 (arXiv 2306.16927) | Comprehensive survey covering modular vs end-to-end, imitation vs RL, open-loop vs closed-loop |

### Automotive AI Platforms

| Resource | Type | Why read it |
|----------|------|-------------|
| ★ Qualcomm Snapdragon Ride Platform | Product documentation | Ride Flex (mixed-criticality ADAS + cockpit on one SoC), Ride Elite (Oryon CPU + Hexagon NPU + Safety Island). 4nm, sampling 2025, production vehicles 2025–2026 |
| NVIDIA DRIVE Orin / Thor | Product documentation | Orin: 254 TOPS, shipping. Thor: 2000 TOPS, targeting 2025. The main competitor to Snapdragon Ride |
| Mobileye EyeQ Ultra | Product documentation | Vision-only ADAS SoC. 176 TOPS. Different philosophy: camera-first, no LiDAR dependency |
| ISO 26262: Road vehicles — Functional safety | Standard | Defines ASIL levels (A–D). Any AI deployed in a vehicle must be argued against this standard |
| SOTIF (ISO 21448): Safety of the Intended Functionality | Standard | Covers hazards from functional insufficiencies (e.g., perception failures) rather than hardware faults. Critical for ML-based ADAS |

---

## Part VII — AI Agents in the Physical World

### Edge-Cloud and On-Device Agents

| Paper | Venue | Why read it |
|-------|-------|-------------|
| ★ Efficient Inference on a Single GPU (Hugging Face) | Tutorial / Blog | Practical guide to KV cache optimization, quantization, and batching for constrained inference |
| MLC-LLM: Universal LLM Deployment Engine (MLC AI) | Open-source, 2023–2024 | Compiles LLMs to run natively on phones and edge devices via TVM Unity. Supports Qualcomm, Apple, and Mali GPUs |
| PowerInfer: Fast Large Language Model Serving with a Consumer-grade GPU (Song et al.) | MLSys 2024 (arXiv 2312.12456) | Exploits neuron activation sparsity for fast LLM inference on consumer hardware. Relevant for edge deployment |

---

## Surveys (Start Here If New to the Field)

| Paper | Venue | Coverage |
|-------|-------|----------|
| ★ An Anatomy of Vision-Language-Action Models (Xu et al.) | arXiv 2512.11362, Dec 2025 | Modules, milestones, and 5 open challenges for VLAs. Continuously updated |
| Pure Vision Language Action (VLA) Models: A Comprehensive Survey (Zhang et al.) | arXiv 2509.19012, Sep 2025 | Classifies 300+ methods by action generation approach. Covers datasets, benchmarks, and simulators |
| A Survey on Efficient Inference for Large Language Models (Zhou et al.) | TMLR 2024 (arXiv 2404.14294) | Covers attention optimization, quantization, speculative decoding, KV cache management, and hardware-aware inference |
| World Models: A Comprehensive Survey (Wang et al.) | arXiv 2606.00133, Jun 2026 | Architectures, reasoning paradigms, and applications of world models across robotics, AV, and video |
| End-to-End Autonomous Driving: Challenges and Frontiers (Chen et al.) | TPAMI 2024 | Modular vs end-to-end, imitation vs RL, evaluation methodology for autonomous driving |

---

## How to Use This List

1. **If you're short on time:** Read only the ★ papers in the parts that match your target role.
2. **If you're preparing for a specific loop:** Cross-reference with the chapter that matches the interview topic; each chapter's header lists its source papers.
3. **If you're writing a chapter:** The papers here are the minimum citations. Check each paper's related-work section for additional references.
4. **Keep current:** Physical AI moves fast. Check arXiv monthly for updates to the VLA, world model, and edge inference categories.
