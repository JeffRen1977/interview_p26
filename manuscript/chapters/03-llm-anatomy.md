# Chapter 3. Anatomy of a Modern LLM

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part II · target ~5,000 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): LLM/LLM算子.md; LLM/softmax.md; LLM/注意力实现.md -->

> A decoder-only transformer walked through operator by operator: embeddings, RMSNorm, attention (MHA, GQA, MLA), RoPE, SwiGLU, and the LM head. For each operator it gives FLOPs, memory traffic, and the numerical pitfalls, including how to make softmax numerically stable. The chapter ends with the architecture trends interviewers probe: longer context, fewer KV heads, and hybrid attention.

## Why interviewers ask this

## What the loops actually ask

- Why did GQA replace MHA?
- Where do the FLOPs and bytes of one forward pass go?
- Why subtract the max in softmax?

## The mental model

## Worked problems

## Code you can run

<!-- softmax / GELU / RoPE (C++) -->

## In the room

## Failure modes

## Exercises
