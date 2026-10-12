# Chapter 9. LLM Anatomy for Physical AI Engineers

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~5,000 words · 5 figures · readiness: Medium — substantial notes, needs narrative and reframing -->
<!-- Private source notes (write fresh; never paste): LLM/注意力实现.md; LLM/MoE.md; LLM/softmax.md; LLM/LLM算子.md -->

> Transformer architecture walked through operator by operator for the Physical AI engineer. LLMs are now part of the robot stack (VLA), the wearable stack (voice), and the IoT stack (edge agents).

## Industry technology (2025–2026)

- **Model families used in Physical AI:** Llama 3.2 (1B/3B for on-device), Gemma 2 (2B/9B, Google), PaliGemma (3B VLM, used in π₀), Phi-3/4 (Microsoft, small models), Qwen 2.5 (Alibaba, 0.5B–72B), Mistral (7B), SmolLM (Hugging Face, 135M–1.7B)
- **Attention variants in production:** GQA (Llama 2/3, Gemma, Mistral), MQA (Falcon, PaLM), MLA (DeepSeek-V2, compressed KV cache), sliding-window attention (Mistral)
- **MoE on edge:** Mixtral 8×7B (sparse, only 2 experts active → ~13B effective), DeepSeek-V2 MoE (236B total, ~21B active); MoE is attractive for edge because active parameters << total
- **Activation functions:** SwiGLU (Llama, Gemma), GeGLU (PaLM), replacing ReLU/GELU in modern transformers
- **Position encoding:** RoPE (Llama, Gemma, Mistral — the standard), ALiBi (legacy), YaRN (long context extension)

## What the loops actually ask

- Walk through one forward pass of a decoder-only transformer (Llama 3.2 3B), operator by operator. For each: FLOPs, memory traffic, and what to watch out for.
- Why did GQA replace MHA? Derive the KV cache memory savings for Llama 3.1 8B (8 KV heads vs 32 Q heads).
- Where do the FLOPs go in one forward pass of a 7B model? Draw the pie chart. Where do the bytes go?
- Why subtract the max in softmax? Write the numerically stable version in C++.
- How does MoE help on-device inference? A Mixtral-style 8×7B model with top-2 routing: how much memory does it use for weights? How much compute per token?
- Explain RoPE. Why is it better than learned position embeddings for long-context Physical AI tasks?
- What is SwiGLU, and why did it replace GELU in modern transformers?

## Companion code

- `ch09/softmax.cpp` — Numerically stable softmax (C++17, subtract-max trick)
- `ch09/rope.cpp` — RoPE (Rotary Position Embedding) applied to Q and K vectors (C++)
- `ch09/swiglu.cpp` — SwiGLU activation: SiLU(gate) × up (C++)
- `ch09/flops_counter.py` — FLOPs and memory traffic breakdown per transformer operator

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
