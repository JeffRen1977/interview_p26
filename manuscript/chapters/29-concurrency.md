# Chapter 29. Concurrency

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part VIII · target ~7,500 words · 6 figures · readiness: Drafted -->
<!-- Private source notes (write fresh; never paste): concurrency/CHAPTER_CONCURRENCY.md (draft; reframe camera examples); concurrency/*.cpp -->

> Thread safety for AI systems: bounded queues, thread pools, read-mostly state such as model and config hot-reload, latest-state caches, token-stream fan-out, deadlock, and how to verify concurrent code. Includes a result about read-mostly locks that surprises most readers.

## Why interviewers ask this

## What the loops actually ask

- Implement a bounded blocking queue.
- Hot-swap a model without blocking inference.
- Find the race.

## The mental model

## Worked problems

## Code you can run

<!-- concurrency suite (C++ + Python) -->

## In the room

## Failure modes

## Exercises
