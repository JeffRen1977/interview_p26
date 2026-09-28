"""Single source of truth for the book's structure.

`python3 gen.py` renders proposal/04-annotated-toc.md and chapter stubs from this file.
Edit chapter titles, abstracts and estimates here, not in the generated files.
"""

TITLE = "From Kernels to Agents"
SUBTITLE = "The Engineering Interview for AI Roles"

PARTS = [
    ("I", "The Landscape",
     "Who is hiring for AI engineering, what the loops look like, and how they are scored."),
    ("II", "Model Foundations an Engineer Must Own",
     "The model-side knowledge that systems interviewers assume: architecture, MoE, decoding, "
     "pre-training and post-training, all explained from the engineer's side."),
    ("III", "Inference Systems",
     "The core of most AI engineering loops: why inference is expensive, and each technique that makes it cheaper."),
    ("IV", "On-Device and Edge AI",
     "Running models where memory, power and heat are the binding constraints."),
    ("V", "AI Infrastructure and Accelerator Software",
     "The software layer between models and hardware: data planes, compilers, and keeping accelerators busy."),
    ("VI", "AI Product System Design",
     "Model-aware system design: the product-facing designs frontier-lab and platform teams ask for, "
     "plus agents and evaluation."),
    ("VII", "Embodied AI",
     "The fastest-growing hiring area outside LLM serving."),
    ("VIII", "The Rounds",
     "Round-specific craft: coding, concurrency, the AI-enabled coding round, and behavioral."),
]

# Fields: num, part, slug, title, est. words, est. figures, abstract, sample questions, companion code,
#         repo sources (private notes; never copied verbatim), readiness
CHAPTERS = [
    (1, "I", "job-map", "The AI Engineering Job Map", 3500, 3,
     "AI hiring has split into at least seven engineering archetypes: frontier-lab product infrastructure, "
     "inference platform, training infrastructure, on-device AI SDK, accelerator software, AI product and "
     "agents, and embodied AI. This chapter explains what each archetype builds, what its loop weighs, and "
     "which parts of the book matter most for each. It also covers how loops changed between 2024 and 2026: "
     "model-aware system design replaced generic system design, and some companies replaced one algorithm "
     "round with an AI-enabled coding round.",
     ["Which AI engineering role fits my background?",
      "Which chapters should I prioritize for a given role?"],
     None,
     ["company/*/README.md", "company/openai/04-tlm-interview-playbook.md"], "new"),
    (2, "I", "scoring", "How AI Loops Are Scored", 4000, 3,
     "The round types are coding, AI-enabled coding, model-aware system design, domain deep-dive, and "
     "behavioral. The chapter covers what interviewers write in their feedback for each, and how the bar "
     "moves from Senior to Staff to Principal: scope, ambiguity, and whether you volunteer the trade-off "
     "before being asked. It closes with 4-week and 8-week study plans keyed to the role archetypes from "
     "Chapter 1.",
     ["What separates a Staff answer from a Senior one?",
      "How should I split limited prep time?"],
     None,
     ["company/openai/tlm-embedded-4week-prep-plan.md", "company/microsoft/26-Microsoft-Principal-ML-Systems面试准备.md §0"],
     "new"),
    (3, "II", "llm-anatomy", "Anatomy of a Modern LLM", 5000, 5,
     "A decoder-only transformer walked through operator by operator: embeddings, RMSNorm, attention (MHA, "
     "GQA, MLA), RoPE, SwiGLU, and the LM head. For each operator it gives FLOPs, memory traffic, and the "
     "numerical pitfalls, including how to make softmax numerically stable. The chapter ends with the "
     "architecture trends interviewers probe: longer context, fewer KV heads, and hybrid attention.",
     ["Why did GQA replace MHA?",
      "Where do the FLOPs and bytes of one forward pass go?",
      "Why subtract the max in softmax?"],
     "softmax / GELU / RoPE (C++)",
     ["LLM/LLM算子.md", "LLM/softmax.md", "LLM/注意力实现.md"], "medium"),
    (4, "II", "moe", "Mixture of Experts", 3500, 3,
     "Sparse MoE from an engineer's point of view: the gating function, top-k routing, load-balancing "
     "losses, capacity factors and token dropping. It then covers what MoE does to systems: expert "
     "parallelism, all-to-all communication, and why MoE inference is memory-bound in a different way from "
     "dense models.",
     ["Why is an MoE with 8x the parameters not 8x the cost?",
      "What breaks when experts are unbalanced?"],
     None,
     ["LLM/MoE.md"], "low"),
    (5, "II", "decoding", "Decoding and Sampling", 3500, 3,
     "Greedy, temperature, top-k and top-p sampling, and why top-p adapts to the shape of the distribution "
     "while top-k does not. It then covers speculative decoding: draft models, the acceptance rule that keeps "
     "outputs unbiased, and the speedup math. The chapter also sets up the decode loop that Part III "
     "optimizes.",
     ["Implement top-p sampling.", "Why is speculative decoding lossless?"],
     "sampling (Python, new)",
     ["LLM/Top-k和Top-p的采样原理.md"], "low"),
    (6, "II", "pretraining", "Pre-Training at Scale", 5000, 5,
     "Data, tensor, pipeline, sequence and expert parallelism, and how they combine into 3D, 4D and 5D "
     "layouts. It covers the collectives each one needs and what they cost, the memory arithmetic behind ZeRO "
     "and activation checkpointing, mixed precision, and distributed checkpointing with fault tolerance. "
     "Scaling laws are presented as a budgeting tool, and MFU as the one number that summarizes all of it.",
     ["How much memory does training a 70B model need?",
      "Choose a parallelism layout for N GPUs.",
      "What limits MFU?"],
     None,
     ["LLM/预训练.md"], "medium"),
    (7, "II", "posttraining", "Post-Training", 5000, 4,
     "SFT and LoRA, reward modeling, RLHF with PPO, DPO and its relatives, and RL from verifiable rewards "
     "(GRPO) behind today's reasoning models. The chapter focuses on what an engineer is asked: the "
     "objectives, what each costs in memory and compute, and how post-training infrastructure differs from "
     "pre-training (rollout generation dominates).",
     ["Derive the DPO loss from the RLHF objective.",
      "Why does GRPO drop the value model?",
      "What is the systems bottleneck in RL post-training?"],
     "DPO loss, GRPO advantage (Python, new)",
     ["LLM/后训练.md"], "medium"),
    (8, "III", "prefill-decode", "The Two Phases: Prefill and Decode", 4000, 4,
     "The single most useful idea in inference interviews: prefill is compute-bound, decode is "
     "memory-bandwidth-bound. The chapter builds the roofline model from first principles, defines TTFT, "
     "TPOT and throughput, and shows how nearly every inference optimization is an attack on one side of "
     "that split.",
     ["Why is decode memory-bound?",
      "Estimate tokens/sec for a model on given hardware."],
     None,
     ["LLM/AI推理.md"], "high"),
    (9, "III", "kv-flash-paged", "KV Cache, FlashAttention, and PagedAttention", 7500, 7,
     "Three techniques, each fixing a cost the previous one exposes. The KV cache removes quadratic "
     "recomputation but creates a memory problem. FlashAttention removes the N×N intermediate with tiling "
     "and online softmax. PagedAttention removes fragmentation with block tables. Each is derived, "
     "implemented in both Python and C++, and tested against a naive reference.",
     ["How big is the KV cache for this model and context?",
      "Derive online softmax.",
      "Why does paging fix fragmentation?"],
     "kv_cache, flash_attention, paged_attention (Python + C++)",
     ["LLM/kv_cache.*", "LLM/flash_attention.*", "LLM/paged_attention.*", "LLM/AI推理.md Part 2"], "high"),
    (10, "III", "kernels", "Kernels: GEMM, the GPU Execution Model, and Fusion", 5500, 6,
     "GEMM optimization as a ladder: naive → loop reordering → cache blocking → SIMD → the GPU's threads, "
     "warps, shared memory and Tensor Cores. Operator fusion follows as the general lesson about memory "
     "traffic, and the chapter finishes with tensor-lifetime analysis and static memory planning, a favorite "
     "question on compiler and runtime teams.",
     ["Optimize this matmul.", "Write a tiled GPU matmul.",
      "Plan buffer offsets from tensor lifetimes."],
     "GEMM ladder (Python + C++), fused kernel, tensor lifetime planner",
     ["LLM/矩阵乘法.md", "LLM/算子融合.md", "LLM/tensor_lifetime.md", "company/microsoft/gemm.*",
      "company/microsoft/01-gemm-cache-simd.md"], "high"),
    (11, "III", "quantization", "Quantization", 4500, 4,
     "Symmetric and asymmetric schemes, per-tensor vs per-channel scales, PTQ vs QAT, and weight-only "
     "methods (AWQ, GPTQ) that make INT4 LLMs practical. The chapter treats numerical debugging as a skill "
     "of its own: how to find the layer that broke accuracy.",
     ["Implement INT8 quantize/dequantize with per-channel scales.",
      "Why does weight-only INT4 help decode but not prefill?",
      "Accuracy dropped 3 points after quantization: find out why."],
     "quantization (C++)",
     ["LLM/量化计算.md", "AI_edge/AI_software_jetski.md Part 2"], "medium"),
    (12, "III", "serving", "Serving at Scale", 4500, 5,
     "From one GPU to a fleet: continuous batching, prefill/decode scheduling and disaggregation, prefix "
     "caching, admission control, GPU job scheduling, load balancing on KV-cache affinity, autoscaling, and "
     "cost per million tokens as the metric the business cares about.",
     ["Design an LLM serving system for 10k QPS.",
      "How does continuous batching work?",
      "Schedule jobs across a GPU cluster."],
     None,
     ["LLM/AI推理.md Parts 1, 5", "company/microsoft/26-*.md", "company/nvidia/01-系统设计.md Q2"], "medium"),
    (13, "IV", "npu-stack", "The NPU Software Stack", 4500, 5,
     "What happens between a trained model and a mobile NPU: the converter, the quantizer, the graph "
     "compiler, the runtime, and CPU/GPU fallback. It covers NPU architecture patterns (vector and tensor "
     "units, tightly coupled memory, VLIW scheduling), custom operators, and profiling across layers. Vendor "
     "SDKs appear as examples; the subject is the pattern they all share.",
     ["Walk through deploying a model to an NPU.",
      "An op falls back to the CPU and latency triples: debug it.",
      "How do you add a custom op?"],
     None,
     ["AI_edge/16-*", "AI_edge/qualcomm_senior_staff_*.md", "AI_edge/AI_software_jetski.md",
      "AI_edge/高通AI软件工程师*.md"], "medium"),
    (14, "IV", "llm-on-phone", "LLMs on a Phone", 4500, 4,
     "Throughput, latency and power together, where DRAM traffic is the currency and thermal throttling is "
     "the deadline. It covers prefill vs decode on a phone, KV cache compression, speculative decoding and "
     "MoE on-device, and the software techniques that cut memory traffic and so power.",
     ["Why does decode drain the battery?",
      "Get a 3B model to 20 tokens/s on a phone.",
      "Reduce DRAM traffic without losing accuracy."],
     None,
     ["AI_edge/27-*"], "high"),
    (15, "IV", "wearables", "Wearables: The One-Watt Problem", 4000, 4,
     "An always-on multimodal agent on a device with a milliwatt budget: wake-up cascades, zero-copy data "
     "paths, and the routing decision between device and cloud. It closes with fleet-scale routing, where "
     "cost and privacy decide what leaves the device.",
     ["Design a multimodal assistant runtime for a wearable.",
      "When should inference run on device vs cloud?"],
     "hybrid routing (Python)",
     ["company/openai/smart-glasses-ai-runtime.md", "company/openai/hybrid-sensor-routing.md"], "high"),
    (16, "V", "data-planes", "Accelerator Data Planes", 4000, 5,
     "How work reaches an accelerator: submission and completion queues, doorbells, DMA descriptors, polling "
     "vs interrupts, user-space drivers, RDMA, host offload cards and NUMA placement. It uses lock-free queue "
     "design as the running example.",
     ["Design a user-space driver data path.",
      "Polling vs interrupts: when does each win?",
      "Why pin memory and threads to a NUMA node?"],
     "SPSC/MPMC queues (C++)",
     ["company/amazon/19-*", "company/amazon/20-*", "company/amazon/21-*", "concurrency/spsc_ring_buffer.*"],
     "medium"),
    (17, "V", "ml-compilers", "ML Compilers and Runtimes", 4000, 4,
     "Framework graph → IR → device binary: capture, lowering, fusion, layout and memory planning, and "
     "scheduling. It then covers the runtime around that binary, including the case for an embedded "
     "scripting layer in the data plane and its traps.",
     ["Walk through what a compiler does to a PyTorch model.",
      "Which fusions are always safe?"],
     None,
     ["company/amazon/23-*"], "medium"),
    (18, "V", "overlap", "Compute–Communication Overlap", 4000, 4,
     "Why backward passes can overlap gradient communication, and how to engineer it: bucketing, double "
     "buffering, chunking, separate streams and hardware fences. MFU runs through the chapter as the "
     "scoreboard.",
     ["MFU is 35%: where did the rest go?",
      "Design an overlapped gradient all-reduce."],
     None,
     ["LLM/LLM训练计算通信重叠与MFU优化.md"], "high"),
    (19, "VI", "model-aware-design", "A Framework for Model-Aware Design", 3500, 3,
     "Why generic system design answers fail in AI loops: tokens per second vs requests per second, GPU "
     "memory as the scarce resource, non-deterministic outputs. The chapter gives a 45-minute answer flow, "
     "the no-buzzword rule, and the 10x/100x scaling drill used in every chapter that follows.",
     ["How is designing an LLM system different?",
      "What should I say in the first five minutes?"],
     None,
     ["company/openai/04-tlm-interview-playbook.md", "company/nvidia/01-系统设计.md (template)"], "medium"),
    (20, "VI", "streaming-chat", "Streaming Chat Backend", 4000, 4,
     "A chat backend for 100M users: choosing between SSE, WebSockets and gRPC streams, cutting TTFT, "
     "cancellation and barge-in that actually frees the GPU, and holding millions of long-lived connections.",
     ["Design a streaming chat backend.",
      "The user hits stop: what happens end to end?"],
     "streaming chat demo (Python)",
     ["company/openai/streaming-chatgpt-backend.md", "company/openai/streaming_chat_demo.py"], "high"),
    (21, "VI", "rate-limiting", "Rate Limiting and Quota for LLMs", 4000, 4,
     "Why requests-per-minute alone fails for LLMs, and how dual RPM+TPM token buckets fix it: reserving "
     "tokens before generation, reconciling afterward, syncing counters across regions, and multi-tenant "
     "fairness for agents that fan out.",
     ["Design a rate limiter for an LLM API.",
      "How do you charge tokens you haven't generated yet?"],
     "token rate limiter (Python)",
     ["company/openai/llm-rate-limiter.md", "company/openai/token_rate_limiter.py"], "high"),
    (22, "VI", "rag", "Retrieval: RAG and Semantic Caching", 4500, 5,
     "Retrieval-augmented generation end to end: ingestion and chunking, embedding and indexing, retrieval "
     "and reranking, and human-in-the-loop document pipelines. Semantic caching is covered as an "
     "optimization with sharp correctness traps (similar question, different answer), including "
     "invalidation when the knowledge base changes.",
     ["Design an enterprise RAG system.", "When is a semantic cache hit wrong?"],
     "semantic cache (Python)",
     ["company/openai/semantic-cache-rag.md", "company/openai/semantic_cache.py",
      "company/nvidia/多 Agent 协同的企业数据检索 RAG 系统",
      "company/nvidia/AI-Powered Document Ingestion & Human-in-the-Loop Pipeline"], "high"),
    (23, "VI", "agents", "Designing Agents", 4000, 4,
     "Tool calling, planning loops, memory (short-term, long-term, shared), multi-agent coordination, "
     "guardrails, and cost control. It emphasizes the questions interviewers push on: how the agent fails, "
     "how you stop it, and how you know it worked.",
     ["Design a coding agent.",
      "How do you keep an agent from looping forever?",
      "Design agent memory."],
     "agent loop with budget and tool sandbox (Python, new)",
     ["company/openai/Memory Design", "company/openai/edge-collaborative-agent-runtime.md"], "low"),
    (24, "VI", "evals", "Evaluating LLM Systems", 4000, 3,
     "Offline eval sets, LLM-as-judge and its biases, regression suites for prompts and agents, online A/B "
     "metrics, and the cost of evaluation itself. Almost every AI product loop asks about evaluation, and "
     "most candidates answer weakly.",
     ["How do you know the new prompt is better?",
      "Design an eval pipeline for a RAG product."],
     "eval harness (Python, new)",
     [], "new"),
    (25, "VI", "sandbox", "Running Untrusted Code", 3500, 4,
     "Code execution for models and agents: the threat model, the isolation spectrum (containers, gVisor, "
     "microVMs), cgroups and default-deny networking, cold-start budgets with warm pools and snapshots.",
     ["Design a multi-tenant code sandbox.",
      "How do you start a sandbox in under 100 ms?"],
     "sandbox orchestrator (Python)",
     ["company/openai/isolated-code-sandbox.md", "company/openai/sandbox_orchestrator.py"], "high"),
    (26, "VI", "realtime-multimodal", "Real-Time Multimodal Agents", 4000, 5,
     "A voice assistant with a 300 ms budget: budgeting latency, why audio uses UDP with FEC instead of TCP, "
     "barge-in with echo cancellation and context rollback, and sensor event loops. It closes with "
     "collaborative agents on unreliable networks.",
     ["Design a real-time voice assistant.", "Where do the 300 ms go?"],
     "voice pipeline, sensor event loop (Python + C++)",
     ["company/openai/realtime-voice-assistant.md", "company/openai/embedded-sensor-event-loop.md",
      "company/openai/voice_assistant_pipeline.py", "company/openai/sensor_event_loop.*"], "high"),
    (27, "VII", "embodied", "Embodied AI and VLA", 4500, 5,
     "Vision-language-action models, diffusion policy and ACT, sim-to-real, world models, benchmarks, and "
     "the data engine behind robot learning: collection, annotation and synthesis. It covers the engineering "
     "questions that come up when a model has to move something.",
     ["Explain a VLA architecture.",
      "Why is robot data the bottleneck?",
      "How do you close the sim-to-real gap?"],
     None,
     ["embodied_AI/18-*", "embodied_AI/embodied_ai_interview_guide.md",
      "embodied_AI/ai_data_annotation_engineering_guide.md"], "medium"),
    (28, "VIII", "coding", "Coding for AI Roles", 4000, 2,
     "The coding round in AI loops: hand-written ML operators (attention, softmax, top-k/top-p, KV cache, "
     "quantize/dequantize), the core algorithm set that still appears, and C++ memory management (pools, "
     "smart pointers). It emphasizes testing against a reference implementation.",
     ["Implement attention from scratch.", "Implement a fixed-size memory pool."],
     "LLM ops, core algorithm set, memory pools (Python + C++)",
     ["LLM/*.py", "LLM/*.cpp", "interview_handwrite/tensor_ops.py", "leetcode/", "c++/"], "high"),
    (29, "VIII", "concurrency", "Concurrency", 7500, 6,
     "Thread safety for AI systems: bounded queues, thread pools, read-mostly state such as model and config "
     "hot-reload, latest-state caches, token-stream fan-out, deadlock, and how to verify concurrent code. "
     "Includes a result about read-mostly locks that surprises most readers.",
     ["Implement a bounded blocking queue.",
      "Hot-swap a model without blocking inference.",
      "Find the race."],
     "concurrency suite (C++ + Python)",
     ["concurrency/CHAPTER_CONCURRENCY.md (draft; reframe camera examples)", "concurrency/*.cpp"], "drafted"),
    (30, "VIII", "ai-native-coding", "The AI-Enabled Coding Round", 7500, 5,
     "The newest round: 60 minutes, an unfamiliar repo, and an AI assistant. The chapter argues that when "
     "code is cheap, the scarce skills are specifying and verifying. It shows those skills through seven "
     "case studies from practice projects, a deep section on differential testing, and a catalog of failure "
     "modes.",
     ["How should I use the AI assistant in the round?",
      "How do I verify code I didn't write?"],
     "7 practice projects with tests (Python + C++)",
     ["AI_native_coding/chapter_ai_native_coding.md (draft, Chinese)", "AI_native_coding/playbook.md",
      "AI_native_coding/*/"], "drafted"),
    (31, "VIII", "behavioral", "Behavioral at Staff Level", 4000, 2,
     "The behavioral signals AI teams screen for: leading ambiguous technical work, disagreeing and "
     "committing, influence without authority, and raising the bar for a team. It gives a theme library, "
     "STAR structure with timing, and patterns that repeat across company value systems.",
     ["Tell me about a technical disagreement.",
      "Tell me about the most ambiguous project you led."],
     None,
     ["behavior/meta-staff-core-themes.md (method only)", "company/nvidia/04-*",
      "company/microsoft/26-*.md §5"], "low"),
]

SAMPLE_CHAPTERS = [9, 21, 30]

APPENDICES = [
    ("A", "numbers", "Numbers and Formulas",
     "FLOPs per token, KV-cache size, memory-bandwidth arithmetic, attention complexity, collective costs, "
     "latency budgets: one sheet for the night before.", 3000),
    ("B", "ml-refresher", "ML Fundamentals Refresher",
     "Loss functions, optimizers, normalization, overfitting and metrics, for systems engineers who have "
     "not trained a model in years.", 4000),
    ("C", "code-index", "Code Index",
     "Every runnable file referenced in the book, with the command that runs it.", 1000),
]
