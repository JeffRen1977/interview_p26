# Chapter 21. Automotive AI: From ADAS to the Software-Defined Vehicle

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: High — based on author's direct experience at Qualcomm -->
<!-- Private source notes (write fresh; never paste): AI_edge/qualcomm_senior_staff_*; AI_edge/16-*; author's Qualcomm automotive AI experience -->

> Why automotive is the highest-stakes Physical AI domain. SoC platforms, ADAS perception, in-cabin AI, digital cockpit, model deployment, safety standards, and OTA updates.

## Industry technology (2025–2026)

- **Qualcomm Snapdragon Ride:**
  - Ride Flex (4nm, mixed-criticality: ADAS + cockpit on one SoC, ASIL-D capable safety island, Hexagon NPU for perception, deployed in 8+ global programs as of CES 2026)
  - Ride Elite (Oryon CPU + Hexagon NPU + Safety Island, sampling 2025, BMW iX3 debut with Ride Pilot at IAA 2025)
  - Digital Chassis platform (cockpit + connectivity + ADAS unified, $100B TAM by 2030 per Qualcomm)
  - QNN SDK for automotive (ONNX → QNN → Hexagon NPU, safety-qualified runtime)
- **NVIDIA DRIVE:**
  - Orin (254 TOPS, shipping in production vehicles), Thor (next-gen, 2000 TOPS, multi-domain compute)
  - DriveWorks SDK (perception libraries), DRIVE OS (safety-certified Linux + QNX hypervisor)
- **Mobileye EyeQ:**
  - EyeQ Ultra (176 TOPS, camera-first), EyeQ 6 (mid/high ADAS)
  - RSS (Responsibility-Sensitive Safety) framework — formal safety model
- **Tesla FSD:**
  - HW4 (2× Trip chips, vision-only), end-to-end neural planner (FSD v12+)
  - Custom training infra (Dojo, now supplemented with NVIDIA H100 clusters)
- **In-cabin AI:** DMS (Driver Monitoring System, EU mandated GSR2 from July 2024), OMS (Occupant Monitoring System), gesture recognition, gaze tracking, natural-language cockpit interaction (Qualcomm + Cerence, NVIDIA + SoundHound)
- **Safety standards:** ISO 26262 (functional safety, ASIL A–D), ISO 21448 SOTIF (Safety of the Intended Functionality — covers AI perception failures), UL 4600 (safety case for autonomous vehicles), UNECE WP.29 (cyber security, OTA regulations)

## What the loops actually ask

- Design the AI perception and cockpit stack for a next-generation vehicle on a Snapdragon Ride Flex platform. Partition workloads across CPU, GPU, NPU, and DSP under a 25W thermal budget in a sealed enclosure at 85°C ambient.
- How do you deploy a perception model (e.g., object detection for ADAS) onto an automotive-grade Hexagon NPU? Walk through the full pipeline: training → quantization → QNN compilation → safety qualification → OTA deployment.
- What is ISO 26262? Explain ASIL levels (A–D). What does ASIL-B mean for a DMS model? What is the difference between ISO 26262 (hardware faults) and SOTIF (functional insufficiencies)?
- A DMS model has a 0.1% false-negative rate for drowsiness detection at 99.9% true-positive rate. At 30 fps, how many frames per minute miss a drowsy driver? Is this safe enough for ASIL-B? Walk through the safety analysis.
- How do you update perception and cockpit models on 500K vehicles already in the field? Design the OTA pipeline. What are the rollback, validation, and safety requirements?
- Compare Qualcomm Snapdragon Ride, NVIDIA DRIVE Thor, and Mobileye EyeQ Ultra as automotive AI platforms. Tradeoffs in compute, power, ecosystem, safety certification?
- The EU mandates DMS (GSR2, July 2024). Design a driver drowsiness + distraction detection system on Snapdragon Ride Flex. Specify: camera placement, model architecture, NPU deployment, latency budget, and ASIL target.
- Ride Flex puts ADAS and cockpit on one SoC. Mobileye and others argue this is risky (thermal, safety isolation). What is the technical argument on each side? How does the Safety Island address it?

## Companion code

- `ch21/auto_workload.py` — Automotive workload partitioning estimator: ADAS, DMS, cockpit across NPU/GPU/CPU/DSP with thermal budget
- `ch21/dms_safety.py` — DMS safety analysis: per-frame FNR → window miss probability → ASIL-B check (with note on correlated failures)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
