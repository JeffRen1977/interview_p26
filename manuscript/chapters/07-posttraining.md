# Chapter 7. Post-Training and Adaptation

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part III · target ~5,000 words · 5 figures · readiness: Medium — existing notes need reframing -->
<!-- Private source notes (write fresh; never paste): LLM/后训练.md; embodied_AI/embodied_ai_interview_guide.md (imitation learning, RL sections) -->

> Fine-tuning foundation models for physical tasks: from general vision to robot manipulation. Imitation learning as post-training (behavior cloning, DAgger, and their failure modes). Reinforcement learning on top of pre-trained representations (reward design is the hard part). RLHF for robots. Diffusion Policy training: denoising score matching for continuous action spaces. Co-training across embodiments (Open X-Embodiment). Domain adaptation across sensors and robots. On-device fine-tuning and personalization with LoRA and adapter layers under memory constraints.

## Why interviewers ask this

## What the loops actually ask

- What is behavior cloning, and what is its well-known failure mode on a real robot?
- Compare imitation learning and RL for manipulation: when do you use each?
- You have a pre-trained VLM and 50 hours of teleoperation data. Design the post-training pipeline to produce a manipulation policy.
- How does Diffusion Policy training work, and why is it suited for continuous action spaces?

## The mental model

## Worked problems

## Code you can run

<!-- behavior cloning baseline (Python) -->

## In the room

## Failure modes

## Exercises
