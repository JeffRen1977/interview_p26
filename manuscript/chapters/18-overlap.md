# Chapter 18. Compute–Communication Overlap

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part V · target ~4,000 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): LLM/LLM训练计算通信重叠与MFU优化.md -->

> Why backward passes can overlap gradient communication, and how to engineer it: bucketing, double buffering, chunking, separate streams and hardware fences. MFU runs through the chapter as the scoreboard.

## Why interviewers ask this

## What the loops actually ask

- MFU is 35%: where did the rest go?
- Design an overlapped gradient all-reduce.

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
