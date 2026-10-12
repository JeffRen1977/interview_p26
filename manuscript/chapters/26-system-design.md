# Chapter 26. System Design for Physical AI

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VIII · target ~7,500 words · 8 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): company/openai/*; company/nvidia/01-*; company/amazon/17-* -->

> The model-aware design method with the Physical AI constraint checklist. Eight fully worked designs using current industry technology.

## Industry technology (2025–2026)

- **Design method:** Start from the model's compute/memory/latency profile (not from boxes and arrows). The constraint checklist: power, thermal, latency, memory, bandwidth, silicon area. Scaling drills: 10× cameras, 100× robots, 1000× sensors.
- **Reference hardware:** Snapdragon 8 Elite (phone), Snapdragon Ride Flex (automotive), Jetson Orin NX (robotics), Apple M4 (wearable/laptop), Coral Edge TPU (IoT), H100/B200 (cloud)
- **Reference models:** Llama 3.2 1B/3B (on-device LLM), Gemini Nano 1.8B, Whisper Small/Medium (ASR), DINOv2 ViT-B (vision), YOLOv8n/s (detection), Depth Anything V2 (monocular depth)
- **Infrastructure:** vLLM/SGLang (cloud serving), ExecuTorch/MLC-LLM (on-device), WebRTC (real-time media), gRPC (inference API), CRDT (offline-first sync), Redis/DragonflyDB (caching)

## What the loops actually ask

Eight fully worked system designs, each using current technology:

### Design 1: Smart Glasses Multimodal Agent Runtime
- Hardware: Snapdragon AR2 Gen 2, 1.5–2.5W total budget
- Models: Whisper Tiny (on-device ASR), Llama 3.2 1B (on-device LLM), DINOv2 ViT-S (vision), cloud fallback to Llama 3.1 70B
- Key challenge: always-on cascade (DSP → NPU → AP → cloud), thermal management, 500ms response budget

### Design 2: 100K Smart Camera Hybrid Routing System
- Hardware: custom SoC per camera (Ambarella CV72 or similar), cloud GPU cluster
- Models: YOLOv8n on-device (INT8), cloud: larger detection + VLM for scene understanding
- Key challenge: confidence-based cascade, bandwidth optimization, fleet-scale cost model

### Design 3: Real-Time Voice Assistant on Embedded Hardware
- Hardware: Snapdragon 8 Elite phone or Qualcomm QCS6490 (IoT)
- Models: Whisper Small (on-device), Llama 3.2 3B (on-device), Kokoro TTS
- Key challenge: <1.5s end-to-end latency, WebRTC for streaming, AEC/VAD pipeline

### Design 4: Robot Fleet Teleoperation Data Pipeline
- Hardware: 100 ALOHA 2 rigs, each with 4 cameras + joint encoders
- Models: ACT policy (training target), CLIP (scene tagging)
- Key challenge: 100 TB/day raw data, quality filtering, real-time monitoring, S3 + data catalog

### Design 5: On-Device LLM with Cloud Fallback
- Hardware: Pixel 9 (12 GB, Tensor G4)
- Models: Gemini Nano 1.8B (on-device), Gemini Pro (cloud)
- Key challenge: router model, KV cache budget, thermal throttling, offline mode

### Design 6: Edge Collaborative Agent Runtime (Offline-Capable)
- Hardware: field devices with intermittent connectivity
- Models: small local LLM (1B), cloud LLM for complex reasoning
- Key challenge: CRDT-based state sync, WAL for durability, conflict resolution, eventual consistency

### Design 7: Perception Pipeline for Autonomous Delivery Robot
- Hardware: Jetson Orin NX, 4 cameras + 1 LiDAR + IMU
- Models: BEVFormer-small (perception), YOLOv8s (detection), Depth Anything V2 (monocular depth)
- Key challenge: 30 fps multi-sensor fusion, 50W power budget, rain/night robustness

### Design 8: AI-Powered Hardware Debugging Agent
- Hardware: cloud-hosted agent, accessing fleet sensor logs
- Models: Llama 3.1 70B (reasoning), retrieval over failure DB, simulation API
- Key challenge: multi-step tool use, grounding in physics, explainability, human-in-the-loop

## Companion code

- `ch26/constraint_checker.py` — System design constraint checker: power, memory, latency budgets (smart glasses example)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
