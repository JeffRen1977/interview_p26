# Chapter 9. KV Cache, FlashAttention, and PagedAttention

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part III · target ~7,500 words · 7 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): LLM/kv_cache.*; LLM/flash_attention.*; LLM/paged_attention.*; LLM/AI推理.md Part 2 -->

> Three techniques, each fixing a cost the previous one exposes. The KV cache removes quadratic recomputation but creates a memory problem. FlashAttention removes the N×N intermediate with tiling and online softmax. PagedAttention removes fragmentation with block tables. Each is derived, implemented in both Python and C++, and tested against a naive reference.

## Why interviewers ask this

## What the loops actually ask

- How big is the KV cache for this model and context?
- Derive online softmax.
- Why does paging fix fragmentation?

## The mental model

## Worked problems

## Code you can run

<!-- kv_cache, flash_attention, paged_attention (Python + C++) -->

## In the room

## Failure modes

## Exercises
