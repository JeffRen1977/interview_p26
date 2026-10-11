# Chapter 14. Model Optimization for Devices

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 4 figures · readiness: Medium — substantial notes -->
<!-- Private source notes (write fresh; never paste): LLM/量化计算.md; AI_edge/27-*; AI_edge/AI_software_jetski.md -->

> Quantization: PTQ, QAT, INT8, INT4, mixed precision. Why quantization is not just "make the numbers smaller" — accuracy cliffs, per-channel vs per-tensor, calibration. Pruning, distillation, and architecture search for edge. Operator fusion and graph optimization. Profiling: how to find the bottleneck (compute-bound vs memory-bound vs launch-bound).

## Why interviewers ask this

## What the loops actually ask

- Explain PTQ vs QAT. When does PTQ fail?
- What is per-channel vs per-tensor quantization, and why does per-channel matter?
- A model quantized to INT8 drops 5% accuracy on one class. Diagnose it.
- How do you profile whether a model is compute-bound or memory-bound on device?

## The mental model

## Worked problems

## Code you can run

<!-- INT8 quantization from scratch (Python + C++), operator fusion example -->

## In the room

## Failure modes

## Exercises
