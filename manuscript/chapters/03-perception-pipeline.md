# Chapter 3. The Perception Pipeline

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part II · target ~5,000 words · 5 figures · readiness: High — restructure existing camera material -->
<!-- Private source notes (write fresh; never paste): camera/3A.md; camera/sensor.md; camera/camera_driver.md -->

> From photons to tensors: sensor physics, Bayer patterns, and the ISP pipeline. Camera 3A (auto-exposure, auto-white-balance, autofocus) as a real-time control system. Multi-sensor systems and sensor fusion fundamentals.

## Industry technology (2025–2026)

- **Image sensors:** Sony IMX890 (50MP, PDAF, stacked BSI), Samsung ISOCELL HP2 (200MP, tetra-pixel), OmniVision OX05B (5MP automotive, LED flicker mitigation)
- **ISP pipelines:** Qualcomm Spectra ISP (18-bit, 3.2 Gpixels/s on Snapdragon 8 Elite), Google Tensor G4 custom ISP (HDRnet, segmentation-aware AE), Apple ISP (Photonic Engine, Deep Fusion)
- **Depth sensors:** Intel RealSense (discontinued 2025 — what replaces it?), Luxonis OAK-D (stereo + Myriad X VPU), Apple LiDAR (dToF, iPad/iPhone Pro), SICK/Velodyne/Ouster (industrial LiDAR)
- **Sensor fusion frameworks:** ROS 2 sensor pipeline (`sensor_msgs`, `image_transport`, `message_filters`), NVIDIA DriveWorks (automotive sensor fusion), Qualcomm camera HAL3 + CamX
- **Robot perception:** Intel RealSense D435i / OAK-D for manipulation, ZED 2i (stereo + IMU), event cameras (Prophesee, for high-speed robotics)

## What the loops actually ask

- Walk through the camera ISP pipeline from raw Bayer sensor data to an RGB tensor ready for neural network inference. What happens at each stage?
- Explain PDAF (phase-detection autofocus) as a closed-loop control system. What is the feedback signal?
- Design a perception pipeline for a robot arm picking objects from a bin using RGB-D + force/torque sensor. Specify the sensors, frame rates, and fusion strategy.
- You have a camera at 30 fps and an IMU at 200 Hz. How do you temporally align them for VIO (visual-inertial odometry)?
- A robot's depth sensor returns noisy point clouds at 15 fps. The policy runs at 10 Hz. Design the data path.
- Compare structured-light, stereo, and time-of-flight depth sensing. When do you use each?

## Companion code

- `ch03/demosaic.py` — Bayer demosaicing from scratch (bilinear interpolation, RGGB pattern)
- `ch03/sensor_align.py` — Temporal alignment of multi-rate sensors (nearest-neighbor and linear interpolation)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
