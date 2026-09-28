# Chapter 8. The Two Phases: Prefill and Decode

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part III · target ~4,000 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): LLM/AI推理.md -->

> The single most useful idea in inference interviews: prefill is compute-bound, decode is memory-bandwidth-bound. The chapter builds the roofline model from first principles, defines TTFT, TPOT and throughput, and shows how nearly every inference optimization is an attack on one side of that split.

## Why interviewers ask this

## What the loops actually ask

- Why is decode memory-bound?
- Estimate tokens/sec for a model on given hardware.

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
