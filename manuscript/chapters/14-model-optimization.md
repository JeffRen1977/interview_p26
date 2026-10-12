# Chapter 14. Model Optimization for Devices

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 4 figures · readiness: Medium — substantial notes -->
<!-- Private source notes (write fresh; never paste): LLM/量化计算.md; AI_edge/27-*; AI_edge/AI_software_jetski.md -->

> Quantization, pruning, distillation, operator fusion, and profiling for edge deployment.

## Industry technology (2025–2026)

- **Quantization methods:** AWQ (MIT, MLSys 2024 Best Paper, activation-aware INT4), GPTQ (ICLR 2023, second-order PTQ), SmoothQuant (ICML 2023, W8A8), QuIP# (2024, incoherence-processed INT2), AQLM (2024, additive quantization), HQQ (half-quadratic quantization, no calibration)
- **Quantization tools:** `autoawq` (Python, one-line INT4 quantization), `auto-gptq`, Qualcomm AIMET (model optimization toolkit), `bitsandbytes` (INT8/INT4 for fine-tuning), llama.cpp `quantize` (GGUF format, Q4_K_M/Q5_K_S etc.)
- **Pruning & distillation:** SparseGPT (2023, unstructured pruning), Wanda (2024, pruning by weights and activations), LLM-Pruner (structured), DistilBERT/TinyLlama (distillation), Phi-3.5 (small-model distillation from larger models)
- **NAS for edge:** Once-for-All (MIT), EfficientFormer (Snap), MobileNetV4 (Google, 2024, Universal Inverted Bottleneck), FastViT (Apple, 2023)
- **Profiling:** `torch.profiler` + TensorBoard, Nsight Systems, Qualcomm Snapdragon Profiler, Android GPU Inspector, Apple Instruments (Metal System Trace)

## What the loops actually ask

- Explain AWQ. Why does protecting 1% of salient weight channels preserve accuracy? How is "salient" determined?
- PTQ vs QAT: when does PTQ fail? Give a concrete example (hint: per-tensor INT8 on attention output with outliers).
- What is per-channel vs per-tensor quantization? Why does per-channel matter? Derive the error difference.
- A model quantized to INT8 drops 5% accuracy on one class (e.g., small objects in detection). Diagnose it. (Hint: activation outliers, calibration data distribution, sensitive layers)
- Compare AWQ, GPTQ, and SmoothQuant. Which is best for on-device LLM deployment? Why?
- You have a 7B model and need to run it on a phone with 6 GB RAM. Walk through the optimization stack: quantization method, format, runtime. Be specific.
- What is mixed-precision quantization? How do you decide which layers get INT8 vs INT4?
- Explain structured vs unstructured pruning. Which actually speeds up inference on an NPU?

## Companion code

- `ch14/awq_quantize.py` — AWQ quantization of Llama 3.2 3B with autoawq (INT4, group size 128)
- `ch14/int8_quant.py` — INT8 symmetric quantization from scratch: per-channel vs per-tensor error comparison

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
