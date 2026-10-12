# Chapter 10. Attention, KV Cache, and Memory

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~7,500 words · 7 figures · readiness: High — working implementations exist -->
<!-- ★ SAMPLE CHAPTER -->
<!-- Private source notes (write fresh; never paste): LLM/kv_cache.*; LLM/flash_attention.*; LLM/paged_attention.* -->

> The KV cache, FlashAttention, PagedAttention, and GQA/MQA. Shows the book's core method: derive, implement, test, then practice the spoken answer.

## Industry technology (2025–2026)

- **FlashAttention evolution:** FlashAttention (Dao, NeurIPS 2022), FlashAttention-2 (ICLR 2024, 2× faster via better parallelism), FlashAttention-3 (2024, Hopper-specific: fp8, warp-specialized async pipelining)
- **PagedAttention & serving:** vLLM (UC Berkeley, PagedAttention, SOSP 2023), SGLang (RadixAttention, prefix caching), TensorRT-LLM (NVIDIA), llama.cpp (local inference with PagedAttention)
- **KV cache optimization:** MLA (Multi-head Latent Attention, DeepSeek-V2 — compresses KV into a latent vector), StreamingLLM (attention sinks for infinite context), H2O (Heavy-Hitter Oracle, KV cache eviction), SnapKV (per-head importance), PyramidKV (adaptive budget)
- **GQA in production:** Llama 3 (8 KV heads / 32 Q heads), Gemma 2 (GQA), Mistral (GQA + sliding window)
- **On-device KV cache:** ExecuTorch KV cache (fixed-size, pre-allocated), MLC-LLM (paged KV on mobile GPU), Apple MLX (unified memory KV cache on M-series)

## What the loops actually ask

- Derive the KV cache memory formula. For Llama 3.1 8B at 8K context with fp16: how many GB?
- Explain FlashAttention. Why is it faster despite doing more FLOPs? Draw the tiling diagram.
- What changed from FlashAttention-2 to FlashAttention-3? Why is FA-3 Hopper-specific?
- What problem does PagedAttention solve? Explain with the virtual memory analogy. What is internal/external fragmentation?
- Compare GQA and MQA. Derive the memory savings. When would you use MQA over GQA?
- You're running Llama 3.2 3B on a phone with 8 GB RAM. The user starts a long conversation. At what context length does the KV cache force you to start evicting? Walk through the math.
- Explain MLA (DeepSeek-V2). How does it compress the KV cache? What is the tradeoff?
- Design a KV cache eviction policy for on-device inference with a hard 512 MB memory limit.

## Companion code

- `ch10/kv_cache.py` — KV cache from scratch with memory growth tracking (Llama 3.1 8B example: 1.07 GB at 8K context)
- `ch10/flash_attention.py` — FlashAttention tiled attention in pure Python (educational: online softmax, block-by-block)
- `ch10/paged_attention.h` — PagedAttention in C++: block allocator, logical-to-physical page table, per-sequence KV management
- `ch10/kv_cache_memory.py` — KV cache memory formula derivation and calculator for GQA/MQA/MLA

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
