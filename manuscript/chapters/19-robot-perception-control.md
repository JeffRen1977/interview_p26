# Chapter 19. Robot Perception and Control

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md -->

> 3D spatial representations: point clouds, NeRF, 3D Gaussian Splatting, 6D pose. SE(3), quaternions, rotation representations for learning. Kinematics and dynamics: FK, IK, Jacobian, impedance control — the minimum a Physical AI engineer needs. Joint-space vs end-effector actions: when to use which. Force sensing, tactile sensing, and contact-rich manipulation. Safety architecture: when a learned policy controls something that can hurt people.

## Why interviewers ask this

## What the loops actually ask

- Design a perception pipeline for 6D pose estimation from RGB-D.
- Explain SE(3) and why quaternions are used for rotation in learning.
- Joint-space vs Cartesian end-effector actions: when do you choose which?
- Walk through the latency budget of a VLA loop running at 10–50 Hz.
- Design the safety architecture for a learned whole-body policy operating near people.

## The mental model

## Worked problems

## Code you can run

<!-- FK/IK example (Python) -->

## In the room

## Failure modes

## Exercises
