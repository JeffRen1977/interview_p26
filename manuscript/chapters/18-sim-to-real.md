# Chapter 18. Sim-to-Real and Robot Learning

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~4,500 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md -->

> Simulation, domain randomization, teacher-student distillation, and closing the sim-to-real gap.

## Industry technology (2025–2026)

- **GPU-parallel simulators:** NVIDIA Isaac Lab (successor to Isaac Gym, GPU-parallel RL with rendering, 2024+), MuJoCo MJX (JAX-compiled physics, 10K+ envs/GPU, DeepMind), Genesis (2024, generative simulation, fast contact), Brax (JAX, simple but fast), PyBullet (legacy, CPU-only)
- **Photorealistic rendering:** NVIDIA Omniverse / Isaac Sim (RTX ray tracing, synthetic data), Blender + BlenderProc (open-source), Unreal Engine (for visual sim-to-real)
- **Sim-to-real success stories:** Agility Robotics Digit (RL in sim → real), Boston Dynamics Atlas (MPC + RL), ETH Zurich ANYmal (teacher-student, Science Robotics 2019), Tesla Optimus (Isaac Sim for manipulation), Google DeepMind humanoid (MuJoCo → real whole-body control)
- **Domain randomization tools:** Isaac Lab randomization API (physics + visual + dynamics), MuJoCo domain randomization (XML-based), NVIDIA Omniverse Replicator (synthetic data generation with randomization)
- **Teacher-student:** Privileged teacher (access to sim-only state: contacts, distances, terrain height maps) → student (observation-only, what the real robot sees). Used in all modern locomotion papers.

## What the loops actually ask

- How do you close the sim-to-real gap? Walk through the systematic approach: domain randomization, system identification, teacher-student distillation.
- A quadruped trained in Isaac Lab walks well in sim but stumbles on real hardware. Debug the gap. What do you check first, second, third?
- Which domain gaps matter most — visual, dynamics, or contact? Give examples of each and how you address them.
- Compare Isaac Lab, MuJoCo MJX, and Genesis for training a contact-rich manipulation policy. What are the tradeoffs in physics fidelity, speed, rendering, and sim-to-real gap?
- Explain teacher-student distillation with privileged information. The teacher sees ground-truth contact forces; the student sees only joint torques and an IMU. How does distillation work?
- You need to train a policy for a dexterous hand (16 DOF). You have Isaac Lab. Design the training pipeline: observation space, action space, reward, domain randomization, and sim-to-real transfer strategy.
- What is system identification? How do you measure the real robot's parameters (friction, damping, motor models) and feed them back into simulation?
- Tesla uses Isaac Sim for Optimus manipulation training. What are the advantages over training on real hardware? What are the risks?

## Companion code

- `ch18/domain_rand.py` — Isaac Lab domain randomization config (physics, actuator, observation noise, visual)
- `ch18/teacher_student.py` — Teacher-student distillation: teacher with privileged info, student with GRU observation history

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
