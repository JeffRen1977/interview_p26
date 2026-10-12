# Chapter 17. Vision-Language-Action Models

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: Medium — substantial notes -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md; embodied_AI/18-* -->

> What is a VLA? The architecture, the models, the action spaces, and the two-speed pattern.

## Industry technology (2025–2026)

- **VLA models:**
  - RT-2 (Google DeepMind, 2023, 55B PaLM-E backbone, tokenized actions, JMLR)
  - π₀ (Physical Intelligence, Oct 2024, 3B PaliGemma + 300M action head, flow matching for continuous actions, up to 50 Hz, 7 embodiments)
  - OpenVLA (Stanford, Jun 2024, 7B, DINOv2 + SigLIP + Llama 2, open-source, LoRA fine-tuning on 1× A100)
  - Octo (UC Berkeley, RSS 2024, 93M, diffusion action head, trained on Open X-Embodiment)
  - RT-1 (Google, RSS 2023, 130K real episodes, tokenized actions, the first large-scale VLA)
- **Diffusion Policy family:** Chi et al. (RSS 2023, DDPM/DDIM for continuous actions), ACT (Action Chunking with Transformers, Stanford/Google), π₀ uses flow matching (a diffusion variant)
- **Frameworks:** LeRobot (Hugging Face, 2024; unified training for ACT, Diffusion Policy, TDMPC), robomimic (Stanford), droid (Berkeley)
- **Hardware platforms:** ALOHA 2 (bimanual), Franka Panda (single-arm), Unitree (humanoid), Hello Robot Stretch, UMI gripper
- **Two-speed pattern:** Slow VLM planner (1–5 Hz, language-conditioned) + fast reactive controller (10–50 Hz, impedance/diffusion policy). Used in SayCan, Code as Policies, Inner Monologue, and π₀

## What the loops actually ask

- Explain a VLA architecture end to end: what goes in (image, language instruction, proprioception), what comes out (actions), and at what control frequency. Draw the diagram.
- Compare RT-2 (55B, tokenized actions, 3 Hz) and π₀ (3B, flow matching, 50 Hz). What are the architectural differences? Why does π₀ use flow matching instead of tokenization?
- OpenVLA uses DINOv2 + SigLIP as the vision encoder. Why two vision backbones? What does each contribute?
- Design a VLA inference loop running at 10 Hz on a robot with an NVIDIA Jetson Orin NX. Walk through the latency budget: image preprocessing, vision encoder, language model forward pass, action head. Where is the bottleneck?
- What is action chunking? Why predict 100 future actions instead of one? What happens if the environment changes mid-chunk?
- Explain the two-speed pattern (slow planner + fast controller). When does it fail? Give a concrete manipulation example.
- You have 200 demonstrations of a bimanual folding task. Which VLA would you fine-tune: OpenVLA (7B) or Octo (93M)? Walk through the tradeoffs.
- How does co-training on Open X-Embodiment data improve single-robot task performance? What are the risks?

## Companion code

- `ch17/vla_inference.py` — VLA inference loop with LeRobot ACT policy: observation, action chunking, control-rate enforcement
- `ch17/vla_comparison.py` — Architecture comparison table: RT-2, π₀, OpenVLA, Octo (params, encoder, action head, control Hz)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
