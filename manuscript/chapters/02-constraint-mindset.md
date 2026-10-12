# Chapter 2. The Constraint Mindset

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part I · target ~4,000 words · 4 figures · readiness: New writing -->

> The five physical constraints — power, thermal, latency, memory, silicon area — and how to reason about them on a whiteboard with back-of-envelope calculations. How interview loops test for constraint awareness by level (Senior, Staff, Principal), and the scoring rubric that separates a weak answer from a Staff-level answer.

## Industry reference points (2025–2026)

- **Power budgets:** Snapdragon 8 Elite NPU: ~5 TOPS/W; Apple ANE (M4): ~18 TOPS at ~8W SoC; NVIDIA Orin: 275 TOPS at 60W; Snapdragon Ride Flex: mixed ADAS+cockpit on ~25W
- **Memory budgets:** Pixel 9: 12 GB shared (OS takes ~4 GB); Galaxy S25: 12 GB; Snapdragon Ride: 16–32 GB LPDDR5
- **Latency budgets:** ADAS object detection: <50 ms sensor-to-decision; voice assistant TTFT: <500 ms; robot control loop: 10–50 Hz (20–100 ms); camera 3A: <33 ms (30 fps)
- **Thermal:** Phone sustained: ~3W SoC; phone burst: ~8W for <30s; automotive: 25W in sealed enclosure at 85°C ambient

## What the loops actually ask

- Given Llama 3.2 3B (3 billion params, 4-bit quantized → ~1.5 GB weights) and a Pixel 9 (12 GB RAM, Tensor G4), estimate whether it fits and at what token rate.
- A Snapdragon 8 Elite phone throttles after 30 seconds of continuous NPU inference. Walk through the thermal analysis and your mitigations.
- You're deploying a DMS model on Snapdragon Ride Flex at ASIL-B. The NPU has 30 TOPS at 5W. The model needs 2 TOPS at 30 fps. Does it fit? What else do you check?
- What separates a Senior answer from a Staff answer on a constraint question? Give an example for power.
- A wearable has a 300 mAh battery and must run always-on inference for 18 hours. What is your power budget for the ML workload?

## Companion code

- `ch02/constraint_check.py` — Back-of-envelope calculator: model memory fit and decode tok/s estimation (Llama 3.2 3B INT4 on Pixel 9 example)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
