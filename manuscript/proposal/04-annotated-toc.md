# Annotated Table of Contents — *From Kernels to Agents*

*The Engineering Interview for AI Roles*

8 parts · 31 chapters · 3 appendices · ~147,500 words (~490 printed pages) · ~130 original figures

Sample chapters are marked ★.

Front matter: a preface on who the book is for and how to use it by role archetype, and a note on the companion code repository.

## Part I — The Landscape

*Who is hiring for AI engineering, what the loops look like, and how they are scored.*

### Chapter 1. The AI Engineering Job Map

AI hiring has split into at least seven engineering archetypes: frontier-lab product infrastructure, inference platform, training infrastructure, on-device AI SDK, accelerator software, AI product and agents, and embodied AI. This chapter explains what each archetype builds, what its loop weighs, and which parts of the book matter most for each. It also covers how loops changed between 2024 and 2026: model-aware system design replaced generic system design, and some companies replaced one algorithm round with an AI-enabled coding round.

Questions the chapter prepares the reader to answer: *Which AI engineering role fits my background?* · *Which chapters should I prioritize for a given role?*

~3,500 words · 3 figures

### Chapter 2. How AI Loops Are Scored

The round types are coding, AI-enabled coding, model-aware system design, domain deep-dive, and behavioral. The chapter covers what interviewers write in their feedback for each, and how the bar moves from Senior to Staff to Principal: scope, ambiguity, and whether you volunteer the trade-off before being asked. It closes with 4-week and 8-week study plans keyed to the role archetypes from Chapter 1.

Questions the chapter prepares the reader to answer: *What separates a Staff answer from a Senior one?* · *How should I split limited prep time?*

~4,000 words · 3 figures

## Part II — Model Foundations an Engineer Must Own

*The model-side knowledge that systems interviewers assume: architecture, MoE, decoding, pre-training and post-training, all explained from the engineer's side.*

### Chapter 3. Anatomy of a Modern LLM

A decoder-only transformer walked through operator by operator: embeddings, RMSNorm, attention (MHA, GQA, MLA), RoPE, SwiGLU, and the LM head. For each operator it gives FLOPs, memory traffic, and the numerical pitfalls, including how to make softmax numerically stable. The chapter ends with the architecture trends interviewers probe: longer context, fewer KV heads, and hybrid attention.

Questions the chapter prepares the reader to answer: *Why did GQA replace MHA?* · *Where do the FLOPs and bytes of one forward pass go?* · *Why subtract the max in softmax?*

~5,000 words · 5 figures · Code: softmax / GELU / RoPE (C++)

### Chapter 4. Mixture of Experts

Sparse MoE from an engineer's point of view: the gating function, top-k routing, load-balancing losses, capacity factors and token dropping. It then covers what MoE does to systems: expert parallelism, all-to-all communication, and why MoE inference is memory-bound in a different way from dense models.

Questions the chapter prepares the reader to answer: *Why is an MoE with 8x the parameters not 8x the cost?* · *What breaks when experts are unbalanced?*

~3,500 words · 3 figures

### Chapter 5. Decoding and Sampling

Greedy, temperature, top-k and top-p sampling, and why top-p adapts to the shape of the distribution while top-k does not. It then covers speculative decoding: draft models, the acceptance rule that keeps outputs unbiased, and the speedup math. The chapter also sets up the decode loop that Part III optimizes.

Questions the chapter prepares the reader to answer: *Implement top-p sampling.* · *Why is speculative decoding lossless?*

~3,500 words · 3 figures · Code: sampling (Python, new)

### Chapter 6. Pre-Training at Scale

Data, tensor, pipeline, sequence and expert parallelism, and how they combine into 3D, 4D and 5D layouts. It covers the collectives each one needs and what they cost, the memory arithmetic behind ZeRO and activation checkpointing, mixed precision, and distributed checkpointing with fault tolerance. Scaling laws are presented as a budgeting tool, and MFU as the one number that summarizes all of it.

Questions the chapter prepares the reader to answer: *How much memory does training a 70B model need?* · *Choose a parallelism layout for N GPUs.* · *What limits MFU?*

~5,000 words · 5 figures

### Chapter 7. Post-Training

SFT and LoRA, reward modeling, RLHF with PPO, DPO and its relatives, and RL from verifiable rewards (GRPO) behind today's reasoning models. The chapter focuses on what an engineer is asked: the objectives, what each costs in memory and compute, and how post-training infrastructure differs from pre-training (rollout generation dominates).

Questions the chapter prepares the reader to answer: *Derive the DPO loss from the RLHF objective.* · *Why does GRPO drop the value model?* · *What is the systems bottleneck in RL post-training?*

~5,000 words · 4 figures · Code: DPO loss, GRPO advantage (Python, new)

## Part III — Inference Systems

*The core of most AI engineering loops: why inference is expensive, and each technique that makes it cheaper.*

### Chapter 8. The Two Phases: Prefill and Decode

The single most useful idea in inference interviews: prefill is compute-bound, decode is memory-bandwidth-bound. The chapter builds the roofline model from first principles, defines TTFT, TPOT and throughput, and shows how nearly every inference optimization is an attack on one side of that split.

Questions the chapter prepares the reader to answer: *Why is decode memory-bound?* · *Estimate tokens/sec for a model on given hardware.*

~4,000 words · 4 figures

### Chapter 9. KV Cache, FlashAttention, and PagedAttention ★

Three techniques, each fixing a cost the previous one exposes. The KV cache removes quadratic recomputation but creates a memory problem. FlashAttention removes the N×N intermediate with tiling and online softmax. PagedAttention removes fragmentation with block tables. Each is derived, implemented in both Python and C++, and tested against a naive reference.

Questions the chapter prepares the reader to answer: *How big is the KV cache for this model and context?* · *Derive online softmax.* · *Why does paging fix fragmentation?*

~7,500 words · 7 figures · Code: kv_cache, flash_attention, paged_attention (Python + C++)

### Chapter 10. Kernels: GEMM, the GPU Execution Model, and Fusion

GEMM optimization as a ladder: naive → loop reordering → cache blocking → SIMD → the GPU's threads, warps, shared memory and Tensor Cores. Operator fusion follows as the general lesson about memory traffic, and the chapter finishes with tensor-lifetime analysis and static memory planning, a favorite question on compiler and runtime teams.

Questions the chapter prepares the reader to answer: *Optimize this matmul.* · *Write a tiled GPU matmul.* · *Plan buffer offsets from tensor lifetimes.*

~5,500 words · 6 figures · Code: GEMM ladder (Python + C++), fused kernel, tensor lifetime planner

### Chapter 11. Quantization

Symmetric and asymmetric schemes, per-tensor vs per-channel scales, PTQ vs QAT, and weight-only methods (AWQ, GPTQ) that make INT4 LLMs practical. The chapter treats numerical debugging as a skill of its own: how to find the layer that broke accuracy.

Questions the chapter prepares the reader to answer: *Implement INT8 quantize/dequantize with per-channel scales.* · *Why does weight-only INT4 help decode but not prefill?* · *Accuracy dropped 3 points after quantization: find out why.*

~4,500 words · 4 figures · Code: quantization (C++)

### Chapter 12. Serving at Scale

From one GPU to a fleet: continuous batching, prefill/decode scheduling and disaggregation, prefix caching, admission control, GPU job scheduling, load balancing on KV-cache affinity, autoscaling, and cost per million tokens as the metric the business cares about.

Questions the chapter prepares the reader to answer: *Design an LLM serving system for 10k QPS.* · *How does continuous batching work?* · *Schedule jobs across a GPU cluster.*

~4,500 words · 5 figures

## Part IV — On-Device and Edge AI

*Running models where memory, power and heat are the binding constraints.*

### Chapter 13. The NPU Software Stack

What happens between a trained model and a mobile NPU: the converter, the quantizer, the graph compiler, the runtime, and CPU/GPU fallback. It covers NPU architecture patterns (vector and tensor units, tightly coupled memory, VLIW scheduling), custom operators, and profiling across layers. Vendor SDKs appear as examples; the subject is the pattern they all share.

Questions the chapter prepares the reader to answer: *Walk through deploying a model to an NPU.* · *An op falls back to the CPU and latency triples: debug it.* · *How do you add a custom op?*

~4,500 words · 5 figures

### Chapter 14. LLMs on a Phone

Throughput, latency and power together, where DRAM traffic is the currency and thermal throttling is the deadline. It covers prefill vs decode on a phone, KV cache compression, speculative decoding and MoE on-device, and the software techniques that cut memory traffic and so power.

Questions the chapter prepares the reader to answer: *Why does decode drain the battery?* · *Get a 3B model to 20 tokens/s on a phone.* · *Reduce DRAM traffic without losing accuracy.*

~4,500 words · 4 figures

### Chapter 15. Wearables: The One-Watt Problem

An always-on multimodal agent on a device with a milliwatt budget: wake-up cascades, zero-copy data paths, and the routing decision between device and cloud. It closes with fleet-scale routing, where cost and privacy decide what leaves the device.

Questions the chapter prepares the reader to answer: *Design a multimodal assistant runtime for a wearable.* · *When should inference run on device vs cloud?*

~4,000 words · 4 figures · Code: hybrid routing (Python)

## Part V — AI Infrastructure and Accelerator Software

*The software layer between models and hardware: data planes, compilers, and keeping accelerators busy.*

### Chapter 16. Accelerator Data Planes

How work reaches an accelerator: submission and completion queues, doorbells, DMA descriptors, polling vs interrupts, user-space drivers, RDMA, host offload cards and NUMA placement. It uses lock-free queue design as the running example.

Questions the chapter prepares the reader to answer: *Design a user-space driver data path.* · *Polling vs interrupts: when does each win?* · *Why pin memory and threads to a NUMA node?*

~4,000 words · 5 figures · Code: SPSC/MPMC queues (C++)

### Chapter 17. ML Compilers and Runtimes

Framework graph → IR → device binary: capture, lowering, fusion, layout and memory planning, and scheduling. It then covers the runtime around that binary, including the case for an embedded scripting layer in the data plane and its traps.

Questions the chapter prepares the reader to answer: *Walk through what a compiler does to a PyTorch model.* · *Which fusions are always safe?*

~4,000 words · 4 figures

### Chapter 18. Compute–Communication Overlap

Why backward passes can overlap gradient communication, and how to engineer it: bucketing, double buffering, chunking, separate streams and hardware fences. MFU runs through the chapter as the scoreboard.

Questions the chapter prepares the reader to answer: *MFU is 35%: where did the rest go?* · *Design an overlapped gradient all-reduce.*

~4,000 words · 4 figures

## Part VI — AI Product System Design

*Model-aware system design: the product-facing designs frontier-lab and platform teams ask for, plus agents and evaluation.*

### Chapter 19. A Framework for Model-Aware Design

Why generic system design answers fail in AI loops: tokens per second vs requests per second, GPU memory as the scarce resource, non-deterministic outputs. The chapter gives a 45-minute answer flow, the no-buzzword rule, and the 10x/100x scaling drill used in every chapter that follows.

Questions the chapter prepares the reader to answer: *How is designing an LLM system different?* · *What should I say in the first five minutes?*

~3,500 words · 3 figures

### Chapter 20. Streaming Chat Backend

A chat backend for 100M users: choosing between SSE, WebSockets and gRPC streams, cutting TTFT, cancellation and barge-in that actually frees the GPU, and holding millions of long-lived connections.

Questions the chapter prepares the reader to answer: *Design a streaming chat backend.* · *The user hits stop: what happens end to end?*

~4,000 words · 4 figures · Code: streaming chat demo (Python)

### Chapter 21. Rate Limiting and Quota for LLMs ★

Why requests-per-minute alone fails for LLMs, and how dual RPM+TPM token buckets fix it: reserving tokens before generation, reconciling afterward, syncing counters across regions, and multi-tenant fairness for agents that fan out.

Questions the chapter prepares the reader to answer: *Design a rate limiter for an LLM API.* · *How do you charge tokens you haven't generated yet?*

~4,000 words · 4 figures · Code: token rate limiter (Python)

### Chapter 22. Retrieval: RAG and Semantic Caching

Retrieval-augmented generation end to end: ingestion and chunking, embedding and indexing, retrieval and reranking, and human-in-the-loop document pipelines. Semantic caching is covered as an optimization with sharp correctness traps (similar question, different answer), including invalidation when the knowledge base changes.

Questions the chapter prepares the reader to answer: *Design an enterprise RAG system.* · *When is a semantic cache hit wrong?*

~4,500 words · 5 figures · Code: semantic cache (Python)

### Chapter 23. Designing Agents

Tool calling, planning loops, memory (short-term, long-term, shared), multi-agent coordination, guardrails, and cost control. It emphasizes the questions interviewers push on: how the agent fails, how you stop it, and how you know it worked.

Questions the chapter prepares the reader to answer: *Design a coding agent.* · *How do you keep an agent from looping forever?* · *Design agent memory.*

~4,000 words · 4 figures · Code: agent loop with budget and tool sandbox (Python, new)

### Chapter 24. Evaluating LLM Systems

Offline eval sets, LLM-as-judge and its biases, regression suites for prompts and agents, online A/B metrics, and the cost of evaluation itself. Almost every AI product loop asks about evaluation, and most candidates answer weakly.

Questions the chapter prepares the reader to answer: *How do you know the new prompt is better?* · *Design an eval pipeline for a RAG product.*

~4,000 words · 3 figures · Code: eval harness (Python, new)

### Chapter 25. Running Untrusted Code

Code execution for models and agents: the threat model, the isolation spectrum (containers, gVisor, microVMs), cgroups and default-deny networking, cold-start budgets with warm pools and snapshots.

Questions the chapter prepares the reader to answer: *Design a multi-tenant code sandbox.* · *How do you start a sandbox in under 100 ms?*

~3,500 words · 4 figures · Code: sandbox orchestrator (Python)

### Chapter 26. Real-Time Multimodal Agents

A voice assistant with a 300 ms budget: budgeting latency, why audio uses UDP with FEC instead of TCP, barge-in with echo cancellation and context rollback, and sensor event loops. It closes with collaborative agents on unreliable networks.

Questions the chapter prepares the reader to answer: *Design a real-time voice assistant.* · *Where do the 300 ms go?*

~4,000 words · 5 figures · Code: voice pipeline, sensor event loop (Python + C++)

## Part VII — Embodied AI

*The fastest-growing hiring area outside LLM serving.*

### Chapter 27. Embodied AI and VLA

Vision-language-action models, diffusion policy and ACT, sim-to-real, world models, benchmarks, and the data engine behind robot learning: collection, annotation and synthesis. It covers the engineering questions that come up when a model has to move something.

Questions the chapter prepares the reader to answer: *Explain a VLA architecture.* · *Why is robot data the bottleneck?* · *How do you close the sim-to-real gap?*

~4,500 words · 5 figures

## Part VIII — The Rounds

*Round-specific craft: coding, concurrency, the AI-enabled coding round, and behavioral.*

### Chapter 28. Coding for AI Roles

The coding round in AI loops: hand-written ML operators (attention, softmax, top-k/top-p, KV cache, quantize/dequantize), the core algorithm set that still appears, and C++ memory management (pools, smart pointers). It emphasizes testing against a reference implementation.

Questions the chapter prepares the reader to answer: *Implement attention from scratch.* · *Implement a fixed-size memory pool.*

~4,000 words · 2 figures · Code: LLM ops, core algorithm set, memory pools (Python + C++)

### Chapter 29. Concurrency

Thread safety for AI systems: bounded queues, thread pools, read-mostly state such as model and config hot-reload, latest-state caches, token-stream fan-out, deadlock, and how to verify concurrent code. Includes a result about read-mostly locks that surprises most readers.

Questions the chapter prepares the reader to answer: *Implement a bounded blocking queue.* · *Hot-swap a model without blocking inference.* · *Find the race.*

~7,500 words · 6 figures · Code: concurrency suite (C++ + Python)

### Chapter 30. The AI-Enabled Coding Round ★

The newest round: 60 minutes, an unfamiliar repo, and an AI assistant. The chapter argues that when code is cheap, the scarce skills are specifying and verifying. It shows those skills through seven case studies from practice projects, a deep section on differential testing, and a catalog of failure modes.

Questions the chapter prepares the reader to answer: *How should I use the AI assistant in the round?* · *How do I verify code I didn't write?*

~7,500 words · 5 figures · Code: 7 practice projects with tests (Python + C++)

### Chapter 31. Behavioral at Staff Level

The behavioral signals AI teams screen for: leading ambiguous technical work, disagreeing and committing, influence without authority, and raising the bar for a team. It gives a theme library, STAR structure with timing, and patterns that repeat across company value systems.

Questions the chapter prepares the reader to answer: *Tell me about a technical disagreement.* · *Tell me about the most ambiguous project you led.*

~4,000 words · 2 figures

## Appendices

**Appendix A. Numbers and Formulas** — FLOPs per token, KV-cache size, memory-bandwidth arithmetic, attention complexity, collective costs, latency budgets: one sheet for the night before. (~3,000 words)

**Appendix B. ML Fundamentals Refresher** — Loss functions, optimizers, normalization, overfitting and metrics, for systems engineers who have not trained a model in years. (~4,000 words)

**Appendix C. Code Index** — Every runnable file referenced in the book, with the command that runs it. (~1,000 words)

