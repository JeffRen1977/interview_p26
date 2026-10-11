# Chapter 18. Sim-to-Real and Robot Learning

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~4,500 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md -->

> Why simulation: data is the bottleneck, and real-robot hours are expensive. GPU-parallel simulation: Isaac Gym/Lab, MuJoCo MJX, Genesis. Domain randomization: visual, dynamics, contact — what to randomize and how much. Teacher-student distillation: privileged information in sim, observation-only in real. System identification and the residual sim-to-real gap.

## Why interviewers ask this

## What the loops actually ask

- How do you close the sim-to-real gap?
- A whole-body controller trained in simulation stumbles on real hardware. Debug the gap.
- Which domain gaps matter most — visual, dynamics, or contact?
- Isaac Sim, MuJoCo, Newton: how do you choose a simulator for a contact-rich manipulation task?
- Explain teacher-student distillation with privileged information.

## The mental model

## Worked problems

## Code you can run

<!-- domain randomization config example -->

## In the room

## Failure modes

## Exercises
