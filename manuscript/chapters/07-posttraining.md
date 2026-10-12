# Chapter 7. Post-Training and Adaptation

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part III · target ~5,000 words · 5 figures · readiness: Medium — existing notes need reframing -->
<!-- Private source notes (write fresh; never paste): LLM/后训练.md; embodied_AI/embodied_ai_interview_guide.md (imitation learning, RL sections) -->

> Fine-tuning foundation models for physical tasks. Imitation learning, RL, diffusion policies, co-training, and on-device adaptation.

## Industry technology (2025–2026)

- **Imitation learning frameworks:** LeRobot (Hugging Face, 2024; open-source robot learning with ACT, Diffusion Policy, TDMPC), robomimic (Stanford), MimicGen (NVIDIA, procedural demo generation)
- **Diffusion Policy:** Chi et al. (RSS 2023), action diffusion for continuous control, DDPM/DDIM schedulers, implemented in LeRobot and robomimic
- **VLA fine-tuning:** OpenVLA fine-tuning with LoRA (7B, fits on 1× A100), π₀ flow matching (3B + 300M action head), Octo fine-tuning (93M, fits on consumer GPU)
- **RL frameworks:** CleanRL (single-file implementations), Stable-Baselines3, Isaac Lab RL (PPO/SAC with GPU-parallel envs), rl_games
- **Efficient fine-tuning:** LoRA/QLoRA (Dettmers, 2023), DoRA (2024), PEFT library (Hugging Face), unsloth (2x faster LoRA)
- **RLHF for robots:** RLHF-based reward learning for manipulation, preference-based RL (PEBBLE), human feedback for safety constraints

## What the loops actually ask

- What is behavior cloning? Explain its compounding error problem on a real robot. How does DAgger address it?
- Compare imitation learning and RL for a bimanual folding task. When do you use each? Can you combine them?
- You have a pre-trained PaliGemma VLM and 50 hours of ALOHA teleoperation data. Design the post-training pipeline to produce a manipulation policy. Be specific about architecture, loss, and training compute.
- How does Diffusion Policy training work? Explain denoising score matching for continuous action spaces. Why is it better than tokenized actions?
- Explain flow matching (as used in π₀). How does it differ from DDPM-style diffusion?
- You want to fine-tune OpenVLA (7B) on your robot's task with 200 demos. You have one A100. Walk through the LoRA setup.
- How does co-training on Open X-Embodiment data improve single-robot performance?

## Companion code

- `ch07/behavior_cloning.py` — ACT policy training with LeRobot on ALOHA sim dataset
- `ch07/vla_lora_finetune.py` — LoRA fine-tuning setup for a 7B VLA (PEFT library, fits 1× A100)
- `ch07/diffusion_policy_train.py` — Diffusion Policy training loop (DDPM scheduler, continuous actions)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
