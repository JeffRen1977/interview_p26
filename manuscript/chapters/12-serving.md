# Chapter 12. Serving at Scale

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part III · target ~4,500 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): LLM/AI推理.md Parts 1, 5; company/microsoft/26-*.md; company/nvidia/01-系统设计.md Q2 -->

> From one GPU to a fleet: continuous batching, prefill/decode scheduling and disaggregation, prefix caching, admission control, GPU job scheduling, load balancing on KV-cache affinity, autoscaling, and cost per million tokens as the metric the business cares about.

## Why interviewers ask this

## What the loops actually ask

- Design an LLM serving system for 10k QPS.
- How does continuous batching work?
- Schedule jobs across a GPU cluster.

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
