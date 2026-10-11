# Chapter 4. Real-Time Data Paths

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part II · target ~4,500 words · 5 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): concurrency/spsc_ring_buffer.*; company/openai/embedded-sensor-event-loop.md -->

> DMA, zero-copy, and dma-buf: moving sensor data without touching the CPU. Ring buffers and producer-consumer patterns for frame pipelines. ISP → NPU → CPU data flow on a mobile SoC. Cache coherency between CPU and accelerators.

## Why interviewers ask this

## What the loops actually ask

- Explain dma-buf and why it matters for on-device inference.
- Design a frame pipeline that feeds camera frames to an on-device model at 30 fps with < 5 ms latency from sensor readout to inference start.
- What is cache coherency, and how does it affect CPU-NPU data sharing?

## The mental model

## Worked problems

## Code you can run

<!-- Lock-free SPSC ring buffer (C++), dma-buf-style zero-copy pipeline -->

## In the room

## Failure modes

## Exercises
