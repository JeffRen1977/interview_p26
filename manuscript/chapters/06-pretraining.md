# Chapter 6. Pre-Training at Scale

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part II · target ~5,000 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): LLM/预训练.md -->

> Data, tensor, pipeline, sequence and expert parallelism, and how they combine into 3D, 4D and 5D layouts. It covers the collectives each one needs and what they cost, the memory arithmetic behind ZeRO and activation checkpointing, mixed precision, and distributed checkpointing with fault tolerance. Scaling laws are presented as a budgeting tool, and MFU as the one number that summarizes all of it.

## Why interviewers ask this

## What the loops actually ask

- How much memory does training a 70B model need?
- Choose a parallelism layout for N GPUs.
- What limits MFU?

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
