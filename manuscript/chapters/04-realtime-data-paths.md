# Chapter 4. Real-Time Data Paths

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part II · target ~4,500 words · 5 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): concurrency/spsc_ring_buffer.*; company/openai/embedded-sensor-event-loop.md -->

> DMA, zero-copy, and dma-buf: moving sensor data without touching the CPU. Ring buffers and producer-consumer patterns for frame pipelines. ISP → NPU → CPU data flow on a mobile SoC. Cache coherency between CPU and accelerators.

## Industry technology (2025–2026)

- **Zero-copy frameworks:** Linux `dma-buf` (shared buffers between ISP, GPU, NPU), Android `AHardwareBuffer` (cross-process, zero-copy GPU/NPU sharing), GStreamer `dmabuf` allocator, Qualcomm FastRPC (DSP↔CPU shared memory)
- **Frame pipeline stacks:** Android CameraX / Camera2 API → HAL3 → CamX → ISP → dma-buf → NPU, Qualcomm SnapDragon Spaces (XR pipeline), NVIDIA DeepStream (video analytics pipeline with zero-copy)
- **Real-time OS/scheduling:** Linux PREEMPT_RT patches (mainlined 6.x), Zephyr RTOS (for MCU-level sensor hubs), QNX (ADAS/automotive real-time), ROS 2 real-time executor (`rclcpp::executors`)

## What the loops actually ask

- Explain `dma-buf` and why it matters for on-device inference. Draw the data flow from camera sensor to NPU.
- Design a frame pipeline that feeds camera frames to an on-device model at 30 fps with < 5 ms latency from sensor readout to inference start. Where are the copies?
- What is cache coherency, and how does it affect CPU-NPU data sharing on a mobile SoC? When do you need explicit cache ops?
- Your robot's camera pipeline drops 1 in 10 frames under load. The sensor runs at 30 fps, the model takes 25 ms. Diagnose the pipeline.
- Compare `AHardwareBuffer` (Android) and `dma-buf` (Linux). When do you use each?
- Design a sensor event loop for a wearable with 3 sensors (camera 30fps, IMU 200Hz, mic 16kHz) feeding one inference pipeline.

## Companion code

- `ch04/spsc_ring_buffer.h` — Lock-free SPSC ring buffer (C++17, power-of-2 size, cache-line aligned head/tail, explicit memory ordering)
- `ch04/latest_value.h` — Latest-value slot: drop old frames, keep newest (double-buffered atomic swap)
- `ch04/zero_copy_pipeline.cpp` — dma-buf-style zero-copy frame pipeline sketch

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
