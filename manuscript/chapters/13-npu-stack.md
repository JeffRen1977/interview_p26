# Chapter 13. The NPU Software Stack

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part IV · target ~4,500 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): AI_edge/16-*; AI_edge/qualcomm_senior_staff_*.md; AI_edge/AI_software_jetski.md; AI_edge/高通AI软件工程师*.md -->

> What happens between a trained model and a mobile NPU: the converter, the quantizer, the graph compiler, the runtime, and CPU/GPU fallback. It covers NPU architecture patterns (vector and tensor units, tightly coupled memory, VLIW scheduling), custom operators, and profiling across layers. Vendor SDKs appear as examples; the subject is the pattern they all share.

## Why interviewers ask this

## What the loops actually ask

- Walk through deploying a model to an NPU.
- An op falls back to the CPU and latency triples: debug it.
- How do you add a custom op?

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
