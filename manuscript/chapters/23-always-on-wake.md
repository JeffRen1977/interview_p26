# Chapter 23. Always-On Cascaded Wake Systems

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VII · target ~4,500 words · 4 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): company/openai/smart-glasses-ai-runtime.md; company/openai/realtime-voice-assistant.md -->

> The power hierarchy (DSP → NPU → AP), cascaded wake, milliwatt budgets, voice assistants on embedded devices, and multimodal always-on sensing.

## Industry technology (2025–2026)

- **Always-on hardware:** Qualcomm Sensing Hub (always-on DSP, <1 mW for wake word, in Snapdragon 8 Elite), Apple Always-On Processor (W-series watch, motion/Siri), Google Tensor context hub (ambient computing)
- **Wake word models:** Porcupine (Picovoice, <500 KB, runs on Cortex-M), TensorFlow Lite Micro (wake word on MCU), Qualcomm AI Keyword Spotter, Amazon AMA (Alexa Mobile Accessory)
- **Voice pipeline:** WebRTC (real-time audio, AEC + VAD + noise suppression), Opus codec (low-latency, 6-510 kbps), Whisper (OpenAI, speech recognition), Moonshine (edge-optimized Whisper alternative)
- **On-device voice assistants:** Apple Siri (on-device speech recognition since iOS 15), Google Assistant (on-device NLU on Tensor), Amazon Alexa (local processing mode)
- **Smart glasses/wearables:** Meta Ray-Ban (Llama on-device for basic tasks, cloud for vision), Google Glass Enterprise (discontinued but reference architecture), Brilliant Labs Frame (open-source AI glasses)
- **Multimodal always-on:** Visual wake (camera-based presence detection, <5 mW), motion wake (IMU-triggered), acoustic event detection (glass break, baby cry)

## What the loops actually ask

- Design the always-on subsystem for smart glasses with a 2W total power budget. The glasses must respond to "Hey glasses" within 500 ms and run for 6 hours on a 700 mAh battery. Walk through the cascaded wake hierarchy.
- Explain the power hierarchy: DSP (µW) → NPU (mW) → AP (W). Why can't you just keep the NPU on all the time?
- How does barge-in work? The user starts talking while the assistant is speaking. Walk through: AEC (acoustic echo cancellation), VAD (voice activity detection), and the audio pipeline.
- Design a multimodal always-on system for a home robot: audio (wake word + acoustic events) + vision (person detection) + motion (vibration). Power budget: 500 mW always-on, 5W burst.
- A smart speaker has a 3% false-wake rate. Users complain. How do you reduce it to <0.5% without increasing miss rate? (Hint: two-stage verification, confidence threshold, personalized models)
- Compare Qualcomm's Sensing Hub approach (dedicated always-on DSP) with a software-only approach (periodic NPU wake). Tradeoffs in power, latency, and accuracy?
- Design the voice pipeline for a wearable: mic → VAD → wake word (DSP) → intent classification (NPU) → full LLM response (AP or cloud). Specify latency at each stage.

## Companion code

- `ch23/wake_power_budget.py` — Cascaded wake power budget calculator (700 mAh wearable, DSP→NPU→AP stages)
- `ch23/voice_pipeline.py` — Voice pipeline latency breakdown: mic → VAD → wake → intent → ASR → LLM → TTS (~1265 ms)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
