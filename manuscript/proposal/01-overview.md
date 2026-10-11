# Book Proposal: *Physical AI: From Sensors to Agents*

**Subtitle:** Building AI Systems That Perceive, Reason, and Act in the Physical World
**Author:** Jeff Ren
**Format:** Print and ebook, ~135,000 words (~450 pages), ~120 original figures, companion code repository (Python + C++, MIT license)
**Status:** Proposal with annotated table of contents; sample chapters in progress

---

## Title

*Physical AI: From Sensors to Agents* captures the book's vertical range: from camera sensors, ISP pipelines and NPU kernels at the bottom, up through VLA models, sim-to-real transfer, and edge-cloud agents at the top. The subtitle says what the book teaches.

Alternates, if the publisher prefers:

- *Physical AI Engineering: From Sensor Pipelines to Embodied Agents*
- *The Physical AI Stack: Perception, Inference, and Action Under Constraints*

## The book in one paragraph

Physical AI is AI that lives in hardware and interacts with the real world: it sees through cameras, hears through microphones, moves robot arms, drives vehicles, and runs on devices you hold or wear. Its defining challenge is not scale — it is *constraints*. Power budgets measured in milliwatts, latency budgets measured in milliseconds, memory budgets measured in megabytes, silicon area budgets measured in square millimeters. This book teaches engineers how to build, optimize, deploy, and debug AI systems that operate under those constraints. It covers the full vertical stack — from sensor physics and real-time data paths, through pre-training and post-training for Physical AI models, inference internals, NPU deployment and hardware-software co-design, to embodied AI (VLA, sim-to-real, robot perception), autonomous systems, and edge-cloud agents. Every core technique is implemented in tested Python and C++. Design chapters give Staff-level answers with the follow-ups interviewers actually ask, and the interview-round chapters cover coding, system design, the AI-enabled coding round, and behavioral.

## Why now

- **Physical AI is the next wave.** NVIDIA's Jensen Huang has called Physical AI the next frontier. Companies like Tesla (Optimus), Figure AI, Physical Intelligence, and dozens of startups are hiring engineers who can bridge AI and hardware. The market for Physical AI engineering talent is growing faster than any other AI segment.
- **No book covers this stack.** Existing AI books cover cloud-side concerns: LLM serving, RAG, recommendation systems, or ML research. None cover the constraint-first engineering that on-device, robotics, and embedded AI teams test for: NPU deployment, sensor pipelines, hardware-software co-design, VLA models, and sim-to-real transfer.
- **The interview loops have changed.** AI engineering interviews at frontier labs, chip vendors, robotics companies, and hyperscalers now probe constraint awareness, on-device inference, and embodied AI — topics no interview book covers.
- **C++ is required, and most AI books are Python-only.** Physical AI roles interview in C++. This book has tested implementations in both languages.

## What makes this book different

1. **Constraint-first.** Every chapter starts from a physical constraint (power, latency, area, bandwidth) and works toward the AI solution, not the other way around. Cloud AI asks "how many GPUs?" Physical AI asks "can this run at 30 Hz on 1.5 watts?"
2. **Full vertical stack.** Sensor physics → ISP → real-time data paths → NPU → model training → inference → agent → actuator, in one book. No other title covers this range.
3. **Runnable, tested code in two languages.** Core techniques (KV cache, FlashAttention, PagedAttention, GEMM, quantization, lock-free queues, ring buffers, hybrid routing) are implemented in Python and C++ and tested. Systems and on-device roles interview in C++, and most competing books do not.
4. **Physical AI training.** Pre-training for vision foundation models and world models, and post-training via imitation learning, RL, and diffusion policies — framed for the engineer who needs to understand the training pipeline that produces the model they will deploy.
5. **Embodied AI coverage.** Four chapters on VLA models, sim-to-real, robot perception and control, and autonomous systems — the topics robotics and Physical AI loops actually ask about.
6. **From the practitioner who shipped it.** The author comes from the constraint side: shipping ML on consumer devices where memory, latency, power, and silicon area are fixed. That is the perspective Physical AI interviews test.

## What the reader will be able to do

- Understand the Physical AI stack from sensor to actuator and identify which role archetype fits them
- Reason about physical constraints (power, thermal, latency, memory, silicon area) on a whiteboard
- Implement attention, the KV cache, FlashAttention, quantization, and lock-free data structures from scratch, in Python or C++
- Explain how a model reaches a mobile NPU, and debug where it goes wrong
- Describe VLA architectures, sim-to-real transfer, and the two-speed planner-controller pattern
- Design edge-cloud hybrid routing, always-on wake cascades, and diagnostic agents for Physical AI systems
- Work effectively in an AI-enabled coding round
- Deliver behavioral answers that show Staff-level scope in Physical AI leadership

## Structure

Eight parts, 27 chapters, three appendices. See `04-annotated-toc.md`.

| Part | Chapters | Focus |
|---|---|---|
| I. The Physical AI Landscape | 1–2 | What Physical AI is; the constraint mindset |
| II. Sensing the Physical World | 3–5 | Perception pipeline, real-time data paths, concurrency |
| III. Training Physical AI Models | 6–8 | Pre-training, post-training and adaptation, data/simulation/evaluation |
| IV. Inference Foundations | 9–12 | LLM anatomy, attention and KV cache, kernels, ML compilers |
| V. AI on the Edge | 13–16 | NPU stack, model optimization, LLMs on devices, HW/SW co-design |
| VI. Embodied AI | 17–20 | VLA models, sim-to-real, robot perception, autonomous systems |
| VII. AI Agents in the Physical World | 21–23 | Hybrid routing, always-on wake, diagnostic agents |
| VIII. The Interview Rounds | 24–27 | Coding, system design, AI-enabled coding, behavioral |
