# Chapter 5. Concurrency for Physical AI

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part II · target ~6,000 words · 5 figures · readiness: High — restructure existing chapter -->
<!-- Private source notes (write fresh; never paste): concurrency/CHAPTER_CONCURRENCY.md; concurrency/*.cpp -->

> Why physical AI is a concurrency problem: multiple sensors, multiple consumers, hard deadlines. Thread pools, lock-free queues, and bounded blocking queues. The latest-value pattern (why you often want to drop frames, not queue them). Deadlock discipline and lock ordering in multi-sensor systems. Real-time scheduling: priority inversion, priority inheritance, RT patches.

## Why interviewers ask this

## What the loops actually ask

- Implement a lock-free SPSC ring buffer.
- When should you drop data instead of queuing it?
- Explain priority inversion and how to prevent it.
- Design a multi-sensor pipeline with three producers and two consumers under real-time constraints.

## The mental model

## Worked problems

## Code you can run

<!-- Thread-safe ring buffer, SPSC queue, bounded blocking queue (C++ and Python) -->

## In the room

## Failure modes

## Exercises
