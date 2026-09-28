# Chapter 11. Quantization

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part III · target ~4,500 words · 4 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): LLM/量化计算.md; AI_edge/AI_software_jetski.md Part 2 -->

> Symmetric and asymmetric schemes, per-tensor vs per-channel scales, PTQ vs QAT, and weight-only methods (AWQ, GPTQ) that make INT4 LLMs practical. The chapter treats numerical debugging as a skill of its own: how to find the layer that broke accuracy.

## Why interviewers ask this

## What the loops actually ask

- Implement INT8 quantize/dequantize with per-channel scales.
- Why does weight-only INT4 help decode but not prefill?
- Accuracy dropped 3 points after quantization: find out why.

## The mental model

## Worked problems

## Code you can run

<!-- quantization (C++) -->

## In the room

## Failure modes

## Exercises
