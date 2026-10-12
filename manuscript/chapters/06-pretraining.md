# Chapter 6. Pre-Training for Physical AI

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part III · target ~5,000 words · 5 figures · readiness: Medium — existing LLM notes need reframing for Physical AI -->
<!-- Private source notes (write fresh; never paste): LLM/预训练.md; LLM/LLM训练计算通信重叠与MFU优化.md; new writing on vision/world models -->

> Why Physical AI models need their own pre-training — the data is not text. Vision foundation models, multi-modal pre-training, world models, and training infrastructure.

## Industry technology (2025–2026)

- **Vision foundation models:** DINOv2 (Meta, 2023; ViT-g, self-supervised, 86.5% linear probe on ImageNet), SigLIP (Google, 2023; sigmoid contrastive loss, efficient batch scaling), EVA-02 (BAAI), InternViT (Shanghai AI Lab)
- **World models:** Genie (Google DeepMind, 2024; playable 2D from video), Genie 2 (2024; 3D scenes from a single image), Genie 3 (2025; real-time 24fps persistent 3D), UniSim (Google, 2024; universal simulator), DIAMOND (2024; diffusion world models for RL)
- **Multi-modal pre-training:** PaLI-X (Google), PaliGemma (Google, 3B, used as π₀'s VLM backbone), LLaVA-OneVision (2024), InternVL2 (Shanghai AI Lab)
- **Training infra:** PyTorch FSDP2 (2024), DeepSpeed ZeRO-3, Megatron-LM (tensor + pipeline parallelism), MosaicML Composer, torchtitan (PyTorch-native distributed training)
- **MFU benchmarks:** H100 achieves ~40-50% MFU for large-scale training; A100 ~30-40%; understanding compute-communication overlap is critical

## What the loops actually ask

- How is pre-training a vision foundation model (DINOv2) different from pre-training an LLM (GPT)? Contrast data, loss function, and output.
- What is a world model? Compare Genie and UniSim. Why does Physical AI need action-conditioned prediction?
- Design a pre-training pipeline for a vision-language model (like PaliGemma) that will later be fine-tuned into a VLA. What data mix, loss, and training schedule?
- What is MFU (Model FLOPs Utilization)? You measure 35% MFU on a 64-GPU run. Walk through how to diagnose and improve it.
- Explain compute-communication overlap in distributed training. Where are the bubbles in pipeline parallelism?
- Your pre-training run on 256 H100s shows 42% MFU. The theoretical peak is ~50%. Where is the other 8%?

## Companion code

- `ch06/mfu_calculator.py` — Model FLOPs Utilization calculator for distributed training (H100/A100 examples)
- `ch06/dinov2_features.py` — DINOv2 ViT-B feature extraction (cls token + spatial patch features)
- `ch06/compute_overlap.py` — Compute-communication overlap visualization for pipeline parallelism

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
