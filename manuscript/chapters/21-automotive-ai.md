# Chapter 21. Automotive AI: From ADAS to the Software-Defined Vehicle

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: High — based on author's direct experience at Qualcomm -->
<!-- Private source notes (write fresh; never paste): AI_edge/qualcomm_senior_staff_*; AI_edge/16-*; author's Qualcomm automotive AI experience -->

> Why automotive is the highest-stakes Physical AI domain: safety-critical, real-time, mass-production. The automotive AI SoC landscape (Qualcomm Snapdragon Ride / Digital Chassis, NVIDIA DRIVE, Mobileye EyeQ, Tesla FSD chip). ADAS perception on an automotive SoC: camera, radar, LiDAR fusion under power and thermal budgets. In-cabin AI: driver monitoring (DMS), occupant monitoring (OMS), gesture, gaze, and voice. Digital cockpit: running multimodal LLMs for natural-language vehicle interaction. Model deployment pipeline from cloud training to automotive-grade NPU. Automotive safety standards (ISO 26262, SOTIF, ASIL levels) and what they mean for AI. OTA model updates. The software-defined vehicle thesis.

## Why interviewers ask this

## What the loops actually ask

- Design the AI perception and cockpit stack for a next-generation vehicle — partition workloads across CPU, GPU, NPU, and DSP under a 25W thermal budget.
- How do you deploy a perception model onto an automotive-grade NPU? Walk through the pipeline.
- What is ISO 26262 and ASIL, and what do they mean for deploying learned models in a vehicle?
- A DMS model has a 0.1% false-negative rate for drowsiness detection. Is that safe enough? Walk through the safety analysis.
- How do you update perception and cockpit models on vehicles already in the field?
- Compare Qualcomm Snapdragon Ride, NVIDIA DRIVE, and Mobileye EyeQ as automotive AI platforms.

## The mental model

## Worked problems

## Code you can run

<!-- automotive workload partitioning example -->

## In the room

## Failure modes

## Exercises
