# Chapter 15. LLMs on Phones and Wearables

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part V · target ~4,500 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): AI_edge/27-*; company/openai/smart-glasses-ai-runtime.md -->

> The memory wall: why a 7B model doesn't fit, and what you do about it. KV cache management under fixed memory: block allocation, eviction, prefix sharing. Speculative decoding on device: draft model on NPU, verify on cloud. Power-aware inference: burst vs sustained, thermal throttling, battery-life SLA.

## Why interviewers ask this

## What the loops actually ask

- Run a 3B-parameter model on a phone with 6 GB RAM, targeting 30 tokens/s decode and < 500 ms TTFT.
- How do you manage the KV cache when total device memory is fixed and shared with the OS?
- Explain speculative decoding. Why is it particularly useful on-device?
- A model inference causes thermal throttling after 30 seconds. What do you do?

## The mental model

## Worked problems

## Code you can run

<!-- on-device KV cache manager -->

## In the room

## Failure modes

## Exercises
