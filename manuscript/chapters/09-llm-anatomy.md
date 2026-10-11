# Chapter 9. LLM Anatomy for Physical AI Engineers

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~5,000 words · 5 figures · readiness: Medium — substantial notes, needs narrative and reframing -->
<!-- Private source notes (write fresh; never paste): LLM/注意力实现.md; LLM/MoE.md; LLM/softmax.md; LLM/LLM算子.md -->

> Transformer architecture walked through operator by operator: what you need to know, and what you can skip. Attention, MLP, layer norm, embeddings — the compute and memory profile of each. MoE: why it matters for edge (route to a smaller expert). Decoding: autoregressive generation, top-k, top-p, temperature. Why this is in a Physical AI book: LLMs are now part of the robot stack (VLA), the wearable stack (voice assistants), and the IoT stack (edge agents).

## Why interviewers ask this

## What the loops actually ask

- Why did GQA replace MHA?
- Where do the FLOPs and bytes of one forward pass go?
- Why subtract the max in softmax?
- How does MoE help on-device inference?

## The mental model

## Worked problems

## Code you can run

<!-- softmax / GELU / RoPE (C++) -->

## In the room

## Failure modes

## Exercises
