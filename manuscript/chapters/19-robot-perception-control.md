# Chapter 19. Robot Perception and Control

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VI · target ~5,000 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md -->

> 3D representations, spatial math, kinematics, control, and safety for learned robot policies.

## Industry technology (2025–2026)

- **3D representations:** 3D Gaussian Splatting (3DGS, SIGGRAPH 2023; real-time novel views, used for robot scene understanding), NeRF (2020, foundational), Nerfstudio (toolbox), FoundationPose (NVIDIA, CVPR 2024; 6D pose without object-specific training), SAM2 (Meta, 2024; promptable segmentation in video)
- **Rotation & spatial math:** `pytorch3d` (Meta, differentiable 3D), `spatialmath-python` (Peter Corke), `liegroups` (for SE(3)), `transforms3d`; Zhou et al. (CVPR 2019) on continuous rotation representations
- **Kinematics & control:** `pinocchio` (efficient rigid-body dynamics, C++ with Python bindings), `mujoco` Python bindings (FK/IK/dynamics), `robotics-toolbox-python` (Peter Corke), Drake (MIT, optimization-based control)
- **Tactile sensing:** GelSight (MIT, vision-based tactile), DIGIT (Meta/GelSight, open-source), ReSkin (magnetic, flexible), BioTac (pressure)
- **Safety frameworks:** ISO 10218 (industrial robot safety), ISO/TS 15066 (collaborative robots, power/force limits), ROS 2 safety controller (`ros2_control` with position/velocity limits), NVIDIA Isaac safety module

## What the loops actually ask

- Design a perception pipeline for 6D pose estimation from RGB-D. You have a bin of novel objects. Specify sensors, models, and the full pipeline. (Hint: FoundationPose or SAM + point cloud registration)
- Explain SE(3) and why rotation representations matter for learning. Why are quaternions bad as regression targets? What does Zhou et al. (CVPR 2019) recommend?
- Joint-space vs Cartesian end-effector actions for a learned manipulation policy: when do you choose which? What are the failure modes of each?
- Compute the forward kinematics for a 6-DOF robot arm. Given joint angles, compute the end-effector pose. (DH parameters or product of exponentials)
- Design the safety architecture for a learned whole-body humanoid policy operating near people. What are the layers? (Hint: workspace limits → velocity limits → force limits → emergency stop → learned policy)
- What is impedance control? Why is it better than position control for contact-rich tasks?
- Compare 3D Gaussian Splatting and NeRF for robot manipulation. When does each win?
- A robot grasps an object but drops it 20% of the time. The perception reports correct pose. Diagnose the system.

## Companion code

- `ch19/forward_kinematics.py` — Forward kinematics with DH parameters (3-DOF planar arm example)
- `ch19/rotation_repr.py` — 6D rotation representation (Zhou et al., CVPR 2019): continuous, no antipodal ambiguity

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
