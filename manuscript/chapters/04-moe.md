# Chapter 4. Mixture of Experts

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part II · target ~3,500 words · 3 figures · readiness: Low — thin notes, mostly new writing -->
<!-- Private source notes (write fresh; never paste): LLM/MoE.md -->

> Sparse MoE from an engineer's point of view: the gating function, top-k routing, load-balancing losses, capacity factors and token dropping. It then covers what MoE does to systems: expert parallelism, all-to-all communication, and why MoE inference is memory-bound in a different way from dense models.

## Why interviewers ask this

## What the loops actually ask

- Why is an MoE with 8x the parameters not 8x the cost?
- What breaks when experts are unbalanced?

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
