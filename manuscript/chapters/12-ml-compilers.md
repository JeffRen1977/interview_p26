# Chapter 12. ML Compilers and Graph Runtimes

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~4,500 words · 4 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): company/amazon/23-Neuron*; LLM/算子融合.md; LLM/tensor_lifetime.md -->

> What an ML compiler does: graph lowering, operator scheduling, memory planning. The compilation pipeline from model to device binary.

## Industry technology (2025–2026)

- **ML compilers:** Apache TVM (Unity, 2024; end-to-end compilation from PyTorch to NPU/GPU), XLA (Google, JAX/TensorFlow backend), Torch Inductor (PyTorch 2.x, `torch.compile`, Triton codegen), MLIR (LLVM infrastructure for ML dialects)
- **Inference runtimes:** TensorRT / TensorRT-LLM (NVIDIA, INT8/FP8 optimization), ExecuTorch (Meta, PyTorch-native on-device, delegates to CoreML/QNN/XNNPACK), ONNX Runtime (Microsoft, cross-platform), MLC-LLM (TVM-based, compiles LLMs to phone GPUs)
- **Vendor-specific stacks:** Qualcomm QNN SDK (AI Engine Direct, Hexagon NPU compiler), Apple CoreML + ANE compiler, Google LiteRT (formerly TFLite), MediaTek NeuroPilot SDK, Samsung ONE (On-device Neural Engine)
- **Memory planning:** Tensor lifetime analysis for fixed-budget devices, ONNX Runtime memory arena, ExecuTorch memory planning pass, TVM relay.transform.InferType + FuseOps + PlanMemory
- **Profiling & debug:** Qualcomm AI Hub (profile models on real devices, cloud-hosted), Netron (model visualization), `torch.export` (graph capture for compilation)

## What the loops actually ask

- Walk through the compilation pipeline from a PyTorch model to a device binary on a Qualcomm Snapdragon NPU (QNN SDK). What happens at each stage?
- What is operator fusion? Give three examples of fusible patterns and one pattern the compiler refuses to fuse. Why?
- A compiled model uses 1.8 GB peak memory. The device has 1.2 GB. Reduce it without changing the model. (Hint: memory planning, operator scheduling, activation checkpointing)
- Compare TensorRT, ExecuTorch, and QNN SDK. When do you use each?
- Explain `torch.compile` and the Inductor backend. What does it do differently from TVM?
- What is `torch.export`? Why is it needed before compiling a model for on-device deployment?
- A model compiles and runs on GPU but produces wrong outputs on NPU. Walk through the debugging process.

## Companion code

- `ch12/torch_compile_demo.py` — `torch.compile` kernel fusion demo (LayerNorm + Linear fused via Inductor)
- `ch12/executorch_export.py` — ExecuTorch export pipeline: `torch.export` → `to_edge` → backend delegation → `.pte`
- `ch12/memory_planning.py` — Tensor lifetime analysis and peak memory estimation

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
