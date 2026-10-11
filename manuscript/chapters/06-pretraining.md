# Chapter 6. Pre-Training for Physical AI

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part III · target ~5,000 words · 5 figures · readiness: Medium — existing LLM notes need reframing for Physical AI -->
<!-- Private source notes (write fresh; never paste): LLM/预训练.md; LLM/LLM训练计算通信重叠与MFU优化.md; new writing on vision/world models -->

> Why Physical AI models need their own pre-training — the data is not text. Vision foundation models (DINOv2, SigLIP, VideoMAE). Multi-modal pre-training: aligning vision, language, and proprioception in a shared embedding space. World models: learning physics from video — prediction, imagination, and planning (Genie, UniSim, DIAMOND). Data engines and pre-training compute (distributed training, compute-communication overlap, MFU optimization). Closes with why this is different from LLM pre-training: multi-modal inputs, action-conditioned prediction, physics grounding, orders of magnitude less data.

## Why interviewers ask this

## What the loops actually ask

- How is pre-training a vision foundation model different from pre-training an LLM?
- What is a world model, and why does Physical AI need one?
- Design a pre-training pipeline for a vision-language model that will be fine-tuned into a VLA.
- What is MFU and how do you optimize it for distributed training?

## The mental model

## Worked problems

## Code you can run

<!-- distributed training overlap example -->

## In the room

## Failure modes

## Exercises
