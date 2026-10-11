# Chapter 13. The NPU Stack

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 5 figures · readiness: Medium — substantial notes, needs narrative -->
<!-- Private source notes (write fresh; never paste): AI_edge/16-*; AI_edge/qualcomm_senior_staff_*.md; AI_edge/AI_software_jetski.md; AI_edge/高通AI软件工程师*.md -->

> What an NPU is and why it exists: ops/watt vs GPU vs CPU. The hardware: MAC arrays, activation buffers, weight SRAM, DMA engines. The software stack: from ONNX/TFLite to compiler IR to hardware instructions. Vendor stacks: Qualcomm QNN/HTP, Apple ANE, Google TPU (Pixel), MediaTek APU, Samsung NPU. How to think about NPU programming without vendor docs.

## Why interviewers ask this

## What the loops actually ask

- Why does an NPU exist alongside the GPU and CPU on a mobile SoC?
- Explain the NPU software stack from model export to hardware execution.
- A model runs 3x slower than expected on the NPU. Debug it.
- What makes a model NPU-friendly vs NPU-hostile?

## The mental model

## Worked problems

## Code you can run

<!-- NPU profiling walkthrough -->

## In the room

## Failure modes

## Exercises
