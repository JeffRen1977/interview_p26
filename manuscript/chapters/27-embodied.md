# Chapter 27. Embodied AI and VLA

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part VII · target ~4,500 words · 5 figures · readiness: Medium — substantial notes, needs narrative and gaps filled -->
<!-- Private source notes (write fresh; never paste): embodied_AI/18-*; embodied_AI/embodied_ai_interview_guide.md; embodied_AI/ai_data_annotation_engineering_guide.md -->

> Vision-language-action models, diffusion policy and ACT, sim-to-real, world models, benchmarks, and the data engine behind robot learning: collection, annotation and synthesis. It covers the engineering questions that come up when a model has to move something.

## Why interviewers ask this

## What the loops actually ask

### VLA architectures
- Explain a VLA architecture end to end: what goes in, what comes out, and at what control frequency.
- What is a vision-language-action model, and how is it different from an LLM with tools?
- Why do systems split into a slow semantic model and a fast control policy instead of one end-to-end network?
- Explain action chunking. Why predict a sequence of future actions instead of the next one?
- Compare RT-2, OpenVLA, π0, and Diffusion Policy: observation/action spaces, data budgets, and where each breaks down.

### Robot data: the bottleneck
- Why is robot data the bottleneck?
- Design a teleoperation data pipeline. Why is data collection the bottleneck rather than compute?
- You have 10 hours of demonstrations and budget for 50 more. How do you decide what to collect, and what return do you expect?
- Behavior cloning on teleoperation data has a well-known failure mode. What is it, and what do you do about it on a real robot?

### Sim-to-real
- How do you close the sim-to-real gap?
- A whole-body controller trained entirely in simulation has to run on real hardware. What transfers, what does not, and how do you close the gap?
- Which domain gaps matter most — visual, dynamics, or contact — and what randomization do you apply to each?
- Isaac Sim/Lab, MuJoCo, Newton: how do you choose a simulator for a contact-rich manipulation task?

### Learning: imitation, RL, world models
- Where does reinforcement learning fit on top of imitation learning for manipulation, and what makes the reward the hard part?
- How do you handle high-dimensional state and action spaces when training RL for manipulation?
- What are the trade-offs between model-free and model-based RL for real-time robot control?

### Perception and control loops
- Design a perception pipeline for object detection, 6D pose estimation, and tracking from RGB-D plus force/torque and tactile sensing.
- Walk through the latency budget of a VLA loop running at 10–50 Hz. Where does the time go?
- Joint-space vs Cartesian end-effector actions: when do you choose which, and what breaks?
- A colleague wants to move the semantic layer to the cloud for a bigger model. Walk through the latency budget.

### Evaluation
- How do you evaluate a manipulation policy when every trial costs robot time and every failure has physical consequences?
- Policy A succeeds 70/100 trials, Policy B 76/100. Is B genuinely better? How do you decide?
- You ship a policy to 300 robots. It works in the lab and degrades in the field. Debug it.
- LIBERO, RLBench, Meta-World, BridgeData: what does each benchmark actually measure, and what do their scores hide?

### Safety
- Design the safety architecture for a learned whole-body policy operating near people.

## The mental model

## Worked problems

## Code you can run

<!-- none planned -->

## In the room

## Failure modes

## Exercises
