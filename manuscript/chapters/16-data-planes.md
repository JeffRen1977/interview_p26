# Chapter 16. Accelerator Data Planes

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part V · target ~4,000 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): company/amazon/19-*; company/amazon/20-*; company/amazon/21-*; concurrency/spsc_ring_buffer.* -->

> How work reaches an accelerator: submission and completion queues, doorbells, DMA descriptors, polling vs interrupts, user-space drivers, RDMA, host offload cards and NUMA placement. It uses lock-free queue design as the running example.

## Why interviewers ask this

## What the loops actually ask

- Design a user-space driver data path.
- Polling vs interrupts: when does each win?
- Why pin memory and threads to a NUMA node?

## The mental model

## Worked problems

## Code you can run

<!-- SPSC/MPMC queues (C++) -->

## In the room

## Failure modes

## Exercises
