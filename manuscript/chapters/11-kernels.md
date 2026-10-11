# Chapter 11. Kernels and GPU Programming

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~5,000 words · 5 figures · readiness: High — existing implementations -->
<!-- Private source notes (write fresh; never paste): LLM/矩阵乘法.md; LLM/LLM算子.md; company/microsoft/01-gemm-* -->

> GEMM: the operation that dominates inference. Memory hierarchy: registers → shared memory → L2 → HBM. Tiling, coalescing, and occupancy. Kernel fusion: why fused kernels win. Profiling with nsight / rocprof.

## Why interviewers ask this

## What the loops actually ask

- Why is GEMM the bottleneck in transformer inference?
- Explain the GPU memory hierarchy and how tiling exploits it.
- When is a kernel compute-bound vs memory-bound, and how do you tell?
- Why does fusing two kernels beat running them separately?

## The mental model

## Worked problems

## Code you can run

<!-- Tiled GEMM (C++), fused attention kernel sketch -->

## In the room

## Failure modes

## Exercises
