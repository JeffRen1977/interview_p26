# Chapter 10. Attention, KV Cache, and Memory

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~7,500 words · 7 figures · readiness: High — working implementations exist -->
<!-- Private source notes (write fresh; never paste): LLM/kv_cache.*; LLM/flash_attention.*; LLM/paged_attention.* -->

> The KV cache: what it is, why it dominates memory, how to manage it. FlashAttention: tiling to stay in SRAM. PagedAttention: virtual memory for the KV cache. Grouped-query attention and multi-query attention: trading compute for memory. Shows the book's core method: derive, implement in Python and C++, test against a reference, then practice the spoken answer.

## Why interviewers ask this

## What the loops actually ask

- Why does the KV cache dominate memory during decode?
- Explain FlashAttention. Why is it faster despite doing more FLOPs?
- What problem does PagedAttention solve, and how?
- When would you choose GQA over MHA on an edge device?

## The mental model

## Worked problems

## Code you can run

<!-- KV cache, FlashAttention, PagedAttention (Python + C++) -->

## In the room

## Failure modes

## Exercises
