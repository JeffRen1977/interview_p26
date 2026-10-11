# Chapter 17. Vision-Language-Action Models

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: Medium — substantial notes -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md; embodied_AI/18-* -->

> What is a VLA? The architecture: vision encoder + language model + action head. RT-2, OpenVLA, Octo, π₀: compare observation spaces, action spaces, data budgets. Action tokenization and action chunking: why predict a trajectory, not a single step. Diffusion Policy and ACT (Action Chunking with Transformers). The two-speed pattern: slow semantic planner + fast reactive controller.

## Why interviewers ask this

## What the loops actually ask

- Explain a VLA architecture end to end: what goes in, what comes out, and at what control frequency.
- What is a vision-language-action model, and how is it different from an LLM with tools?
- Compare RT-2, OpenVLA, π₀, and Diffusion Policy.
- Design a VLA loop running at 10–50 Hz. Walk through the latency budget.
- Why do systems split into a slow semantic model and a fast control policy?

## The mental model

## Worked problems

## Code you can run

<!-- VLA inference loop sketch -->

## In the room

## Failure modes

## Exercises
