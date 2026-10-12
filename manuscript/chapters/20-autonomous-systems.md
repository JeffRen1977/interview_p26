# Chapter 20. Autonomous Systems

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~4,500 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): company/nvidia/03-AV*; company/nvidia/01-系统设计.md -->

> Self-driving, sensor suites, BEV perception, end-to-end vs modular, and evaluation.

## Industry technology (2025–2026)

- **AV stacks:**
  - Waymo (5th-gen Driver, LiDAR + camera + radar, modular but moving toward end-to-end components)
  - Tesla FSD (HW4: 2× Trip chip, vision-only, end-to-end neural planner deployed since v12)
  - Cruise (post-pause restructuring, LiDAR + camera)
  - Mobileye (EyeQ Ultra, 176 TOPS, camera-first RSS safety)
  - NVIDIA DRIVE (Orin: 254 TOPS, Thor: 2000 TOPS, DriveWorks SDK)
  - Qualcomm Snapdragon Ride (see Ch 21)
- **BEV perception:** BEVFormer (ECCV 2022, spatiotemporal transformers), BEVDet, BEVFusion (MIT, camera + LiDAR), StreamPETR (streaming BEV)
- **End-to-end driving:** UniAD (CVPR 2023 Best Paper, planning-oriented), VAD (vectorized, no HD map), GameFormer (game-theoretic prediction + planning), Tesla's end-to-end neural network planner (production since FSD v12)
- **Evaluation:** nuScenes (benchmark, NDS metric), Waymo Open Dataset, CARLA (open-source simulator), NAVSIM (real-world navigation simulation)
- **Edge cases:** 3D occupancy networks (replacing object detection for rare classes), world models for scenario generation (Waymo World Model with Genie 3)

## What the loops actually ask

- Walk through the autonomous driving stack from sensor to actuator. Draw the block diagram. Where is ML used at each stage?
- What is BEV (Bird's Eye View) representation? Why do modern perception stacks use it instead of 2D detection? Explain BEVFormer's approach.
- End-to-end (Tesla FSD v12) vs modular (Waymo): what are the trade-offs? Which is winning and why?
- Design the perception stack for a sidewalk delivery robot (Nuro/Serve/Starship-style). Constraints: outdoor, pedestrians, $500 sensor budget, 50W compute.
- How do you handle long-tail edge cases in autonomous driving? Give 3 concrete examples and your mitigation for each.
- Tesla removed radar and went vision-only. What are the technical arguments for and against?
- Explain occupancy networks. Why are they replacing 3D object detection for some use cases?
- Compare UniAD and VAD as end-to-end driving architectures. What does each do differently?

## Companion code

- `ch20/bev_projection.py` — BEV projection: 3D points to bird's-eye-view occupancy grid (LiDAR example)
- `ch20/sensor_suite.py` — Sensor suite comparison table: Waymo Gen5, Tesla HW4, budget delivery robot

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
