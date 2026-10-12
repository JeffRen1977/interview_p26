# Chapter 11. Kernels and GPU Programming

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~5,000 words · 5 figures · readiness: High — existing implementations -->
<!-- Private source notes (write fresh; never paste): LLM/矩阵乘法.md; LLM/LLM算子.md; company/microsoft/01-gemm-* -->

> GEMM, memory hierarchy, tiling, kernel fusion, and profiling. The GPU kernel skills that Physical AI inference demands.

## Industry technology (2025–2026)

- **GPU hardware:** NVIDIA H100 (3.35 PFLOPS fp8, 3.35 TB/s HBM3), H200 (141 GB HBM3e), B200 (4.5 PFLOPS fp8), A100 (reference for most published work); AMD MI300X (192 GB HBM3); Apple M4 (unified memory, Metal)
- **Kernel frameworks:** Triton (OpenAI, write GPU kernels in Python, 2.x), CUTLASS (NVIDIA, C++ GEMM templates), CuTe (NVIDIA, layout algebra), ThunderKittens (Stanford, 2024, tile-level DSL for H100)
- **Profiling tools:** NVIDIA Nsight Compute (kernel-level), Nsight Systems (system-level), `torch.profiler` with TensorBoard, AMD ROCm Profiler, Apple Metal GPU Capture
- **Kernel fusion in practice:** FlashAttention (fused attention), xformers (Meta, fused attention + MLP), Unsloth (fused LoRA kernels, 2× faster fine-tuning), torch.compile (inductor backend auto-fusion)
- **Roofline model:** The standard tool for classifying kernels as compute-bound (GEMM with large M,N,K) vs memory-bound (decode attention, LayerNorm, element-wise ops)

## What the loops actually ask

- Why is GEMM the bottleneck in transformer inference? When is it not?
- Draw the GPU memory hierarchy (registers → shared memory → L2 → HBM). How does tiling exploit it?
- Write a tiled matrix multiply in Triton. Explain the tile size choice.
- When is a kernel compute-bound vs memory-bound? Draw the roofline model for H100. Where does attention decode fall? Where does GEMM fall?
- Why does fusing LayerNorm + Linear beat running them separately? Quantify the HBM savings.
- Explain warp divergence. Give a Physical AI kernel example where it matters.
- A custom kernel runs at 40% of peak FLOPS on H100. How do you diagnose whether it's compute-bound, memory-bound, or latency-bound?
- What is ThunderKittens? How does tile-level programming differ from warp-level CUDA?

## Companion code

- `ch11/triton_gemm.py` — Tiled GEMM kernel in Triton (block-level tiling, masking, accumulator in fp32)
- `ch11/roofline.py` — Roofline analysis: classify kernels as compute-bound or memory-bound (H100 examples)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
