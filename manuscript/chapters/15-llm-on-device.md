# Chapter 15. LLMs on Phones and Wearables

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): AI_edge/27-*; company/openai/smart-glasses-ai-runtime.md -->

> The memory wall, KV cache management on fixed memory, speculative decoding on device, and power-aware inference.

## Industry technology (2025–2026)

- **On-device LLM runtimes:** llama.cpp / llama-cli (GGUF format, Metal/Vulkan/CUDA backends, runs Llama 3.2 3B on phones), MLC-LLM (TVM-compiled, Vulkan/Metal/OpenCL, web/iOS/Android), ExecuTorch (PyTorch-native, QNN/CoreML delegates), MediaPipe LLM Inference (Google, cross-platform), Apple MLX (M-series, unified memory)
- **Models shipping on-device:** Gemini Nano (Google Pixel 9, 1.8B), Apple Intelligence (on-device ~3B for Siri/writing), Samsung Galaxy AI (on-device summarization), Llama 3.2 1B/3B (Meta, designed for mobile)
- **KV cache on device:** Fixed-size pre-allocated (ExecuTorch), sliding window (Mistral-style, constant memory), StreamingLLM (attention sinks), SnapKV (per-head importance pruning), quantized KV cache (INT8 KV, 50% memory reduction)
- **Speculative decoding on device:** Draft model on NPU (small 100M model) + verify on GPU (main model), Medusa heads (no draft model needed), EAGLE (feature-level drafting), Lookahead decoding
- **Thermal management:** DVFS (Dynamic Voltage and Frequency Scaling), thermal throttling curves, burst vs sustained compute budgets, Android thermal HAL (`IThermal`), iOS thermal state notifications

## What the loops actually ask

- Run Llama 3.2 3B on a Pixel 9 (12 GB RAM, Tensor G4). Target: 30 tok/s decode, <500 ms TTFT. Walk through the full deployment: quantization, runtime, KV cache budget, and thermal constraints.
- The KV cache for a 3B model at 4K context uses 512 MB. The phone has 3 GB available for inference. Design the memory allocation strategy. What happens at 8K context?
- Explain speculative decoding. You have a 3B main model and a 100M draft model. The draft model runs on NPU (5 ms/token), the main model on GPU (30 ms/token). What is the expected speedup with acceptance rate 0.8?
- A user runs the on-device LLM for 60 seconds. After 30 seconds, the SoC hits 45°C and throttles the GPU from 900 MHz to 600 MHz. Tokens/sec drops from 30 to 18. What do you do?
- Compare llama.cpp, MLC-LLM, and ExecuTorch for deploying a 3B model on Android. Tradeoffs?
- Design the KV cache eviction policy for an on-device chatbot with a 512 MB hard memory limit. The user is in a long conversation (10K+ tokens). What do you keep? What do you evict?
- How does quantized KV cache (INT8 instead of FP16) affect quality vs memory? When is it safe?

## Companion code

- `ch15/device_memory_budget.py` — On-device memory budget calculator: weights + KV cache + activations, max context estimator (Llama 3.2 3B on Pixel 9 example)
- `ch15/spec_decode_speedup.py` — Speculative decoding expected speedup calculator (NPU draft + GPU main model)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
