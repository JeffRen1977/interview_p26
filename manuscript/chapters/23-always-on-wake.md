# Chapter 23. Always-On Cascaded Wake Systems

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VII · target ~4,500 words · 4 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): company/openai/smart-glasses-ai-runtime.md; company/openai/realtime-voice-assistant.md -->

> The power hierarchy: DSP (µW) → NPU (mW) → AP (W). Wake word detection → intent classification → full model activation. Designing for milliwatt budgets: what can run always-on, what must be burst. Voice assistants on embedded devices: AEC, VAD, barge-in, WebRTC. Multi-modal always-on: audio + motion + visual change detection.

## Why interviewers ask this

## What the loops actually ask

- Design the always-on subsystem for smart glasses with a 2W total budget.
- Explain the cascaded wake hierarchy and why each stage exists.
- How does barge-in work, and why is AEC critical?
- Design a multimodal assistant runtime for a wearable.

## The mental model

## Worked problems

## Code you can run

<!-- wake cascade state machine -->

## In the room

## Failure modes

## Exercises
