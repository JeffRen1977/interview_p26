# Chapter 16. Hardware-Software Co-Design

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- ★ SAMPLE CHAPTER -->
<!-- Part V · target ~4,500 words · 4 figures · readiness: High — based on author's direct experience -->
<!-- Private source notes (write fresh; never paste): behavior/meta-staff-core-themes.md (stories 1–2); camera HW content -->

> The thesis: the best optimization is the one you never have to make at runtime. Co-designing the algorithm and the silicon.

## Industry technology (2025–2026)

- **Algorithm-silicon co-design examples:** Google Tensor (custom ISP + ML pipelines co-designed), Apple Neural Engine (operators co-designed with CoreML compiler), Qualcomm Hexagon (VTCM tiling co-designed with QNN compiler), Tesla FSD chip (custom MAC array for their network architecture)
- **Fixed-function accelerators:** Google Pixel Visual Core / Tensor ISP (HDR+, Night Sight), Apple ProRes encoder (dedicated silicon), NVIDIA NVDLA (open-source deep learning accelerator), Qualcomm Spectra ISP (3A + segmentation in fixed-function)
- **HW/SW interface tools:** Chisel (Scala-based HDL), SystemVerilog + UVM (verification), Verilator (simulation), NVIDIA NVDLA compiler (reference for HW/SW interface), MLIR hardware dialects
- **Design methodology:** RTL-to-GDS flow, power-performance-area (PPA) analysis, timing closure, silicon validation, algorithm team ↔ RTL team interface contracts
- **Case study reference:** PDAF (Phase Detection Autofocus) — algorithm changes that reduced silicon area by 47% across 4 Pixel generations. The principle: constrain the algorithm to what hardware does cheaply, not the other way around.

## What the loops actually ask

- When should compute move from the AP (CPU/GPU) to dedicated hardware? Give the decision framework.
- You're given a 2 mm² silicon area budget for an on-chip accelerator. Walk through how you decide what to put in it. What is the process for working with the hardware team?
- What is the trade-off between fixed-function and programmable silicon? Give examples from mobile SoCs.
- Explain a HW/SW co-design project you led. What was the algorithm change, what was the hardware change, and what was the result? (This is the PDAF story)
- A neural network operator (e.g., depthwise convolution with a specific kernel size) is 5× slower on the NPU than expected. How do you determine whether the fix is in the algorithm, the compiler, or the hardware?
- Google's Tensor chip has custom ISP silicon for computational photography. Why not just run it on the GPU? Walk through the power/latency/area analysis.
- Tesla designed their FSD chip with a custom neural network accelerator rather than using an off-the-shelf GPU. What are the advantages and risks?

## Companion code

- `ch16/area_estimator.py` — Silicon area estimator for a systolic MAC array + weight/activation SRAM
- `ch16/hw_sw_decision.py` — Fixed-function vs programmable decision matrix (scoring by algorithm stability, power, volume, latency)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
