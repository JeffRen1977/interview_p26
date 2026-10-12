# Chapter 13. The NPU Stack

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 5 figures · readiness: Medium — substantial notes, needs narrative -->
<!-- Private source notes (write fresh; never paste): AI_edge/16-*; AI_edge/qualcomm_senior_staff_*.md; AI_edge/AI_software_jetski.md; AI_edge/高通AI软件工程师*.md -->

> What an NPU is, why it exists, the hardware, and the vendor software stacks.

## Industry technology (2025–2026)

- **Qualcomm Hexagon NPU:** Snapdragon 8 Elite (Hexagon NPU, 45 TOPS INT8, 3rd-gen micro-tile inference), QNN SDK (AI Engine Direct, supports ONNX/TFLite/PyTorch via DLC format), AI Hub (cloud-hosted profiling on 100+ real devices)
- **Apple ANE:** A18 Pro (35 TOPS), M4 (38 TOPS), CoreML compiler (MIL intermediate representation), private API but public CoreML framework; FP16 only (no INT4 on ANE as of 2025)
- **Google Tensor/TPU:** Tensor G4 (Pixel 9, custom TPU for on-device, LiteRT runtime), Edge TPU (Coral, 4 TOPS INT8), Gemini Nano on-device
- **MediaTek APU:** Dimensity 9400 (NPU 6.0, 46 TOPS), NeuroPilot SDK
- **Samsung NPU:** Exynos 2500 (dual-core NPU), ONE compiler (Samsung Neural Network compiler)
- **NVIDIA for edge:** Jetson Orin NX (100 TOPS, for robotics), Isaac platform, TensorRT
- **NPU hardware concepts:** Systolic arrays (Google TPU), MAC arrays with weight-stationary dataflow (Qualcomm HTP), activation buffers, weight SRAM, DMA engines for streaming

## What the loops actually ask

- Why does an NPU exist alongside the GPU and CPU on a mobile SoC? Draw the ops/watt comparison.
- Walk through the QNN SDK pipeline: PyTorch model → ONNX export → QNN model library → device execution. What happens at each stage?
- A model runs 3× slower than expected on the Hexagon NPU. Walk through the debugging process. (Hint: unsupported ops fall back to CPU, quantization mismatch, memory bandwidth)
- What makes a model "NPU-friendly"? Give 5 specific architectural choices that help and 3 that hurt.
- Compare Qualcomm QNN, Apple CoreML, and Google LiteRT. Which models run well on each? Where do they differ?
- You're deploying a DINOv2 ViT-B on Snapdragon 8 Elite. The model has LayerNorm, GELU, and dynamic shapes. Which ops will the NPU handle natively, and which will fall back?
- Explain weight-stationary vs output-stationary dataflow in a MAC array. When does each win?
- How does Qualcomm AI Hub help you debug NPU deployment? Walk through profiling a model.

## Companion code

- `ch13/export_to_qnn.py` — Export a PyTorch model to QNN-compatible ONNX (static shapes, NPU-friendly ops)
- `ch13/qnn_compile.sh` — QNN SDK compilation pipeline: ONNX → `qnn-onnx-converter` → `qnn-model-lib-generator` → `qnn-net-run`

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
