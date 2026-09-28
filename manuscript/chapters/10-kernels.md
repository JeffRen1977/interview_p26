# Chapter 10. Kernels: GEMM, the GPU Execution Model, and Fusion

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part III · target ~5,500 words · 6 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): LLM/矩阵乘法.md; LLM/算子融合.md; LLM/tensor_lifetime.md; company/microsoft/gemm.*; company/microsoft/01-gemm-cache-simd.md -->

> GEMM optimization as a ladder: naive → loop reordering → cache blocking → SIMD → the GPU's threads, warps, shared memory and Tensor Cores. Operator fusion follows as the general lesson about memory traffic, and the chapter finishes with tensor-lifetime analysis and static memory planning, a favorite question on compiler and runtime teams.

## Why interviewers ask this

## What the loops actually ask

- Optimize this matmul.
- Write a tiled GPU matmul.
- Plan buffer offsets from tensor lifetimes.

## The mental model

## Worked problems

## Code you can run

<!-- GEMM ladder (Python + C++), fused kernel, tensor lifetime planner -->

## In the room

## Failure modes

## Exercises
