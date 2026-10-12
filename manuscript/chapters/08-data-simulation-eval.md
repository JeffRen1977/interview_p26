# Chapter 8. Data, Simulation, and Evaluation

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part III · target ~5,000 words · 5 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): embodied_AI/embodied_ai_interview_guide.md; embodied_AI/ai_data_annotation_engineering_guide.md -->

> The data bottleneck, teleoperation, synthetic data, evaluation, and benchmarks for Physical AI.

## Industry technology (2025–2026)

- **Teleoperation hardware:** ALOHA 2 (Stanford/Google, bimanual, $20K; open-source), Gello (universal controller, $500), UMI (Universal Manipulation Interface, portable gripper), VR controllers (Meta Quest 3 for teleop), exoskeletons (for humanoid teleoperation)
- **Simulators:** NVIDIA Isaac Lab (GPU-parallel RL, domain randomization, rendering), MuJoCo MJX (JAX-compiled physics, 10K+ envs/GPU), Genesis (2024; generative simulation), NVIDIA Omniverse/Isaac Sim (photorealistic rendering), PyBullet
- **Synthetic data:** NVIDIA Isaac Sim procedural scene generation, Infinigen (procedural 3D worlds), Objaverse (800K+ 3D objects), Google SynTable (tabletop data)
- **Benchmarks:** LIBERO (130 tasks, 4 suites, NeurIPS 2023), RLBench (100 tasks), SIMPLER (Google, real-world eval of sim-trained policies), BridgeData V2 (60K real demos), Meta-World (50 tasks), CALVIN (long-horizon language-conditioned)
- **Data quality tools:** rerun.io (multimodal data visualization), Weights & Biases (experiment tracking), Label Studio (annotation)

## What the loops actually ask

- Why is robot data the bottleneck rather than compute? Compare to LLM data scaling.
- Design a teleoperation data pipeline for a bimanual manipulation task. Specify: hardware, recording format, quality metrics, and target demos/hour.
- You have 10 hours of demonstrations and budget for 50 more hours. Design the data collection strategy — what tasks, what diversity, what quality checks?
- Policy A succeeds 70/100 trials, Policy B 76/100. Is B genuinely better? Walk through the statistical test. (Hint: binomial confidence interval, Fisher exact test)
- Compare Isaac Lab and MuJoCo MJX for training a locomotion policy. What are the tradeoffs in physics fidelity, speed, and sim-to-real gap?
- How does MimicGen scale teleoperation data? What are its limits?
- You trained a policy in simulation that gets 95% success. In the real world it gets 40%. Walk through the debugging process.

## Companion code

- `ch08/policy_comparison.py` — Statistical comparison of two robot policies (Wilson CI, Fisher exact test; 70/100 vs 76/100 example)
- `ch08/domain_rand_config.py` — Isaac Lab domain randomization config (physics, visual, dynamics parameters)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
