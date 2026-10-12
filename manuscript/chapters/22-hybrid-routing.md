# Chapter 22. Edge-Cloud Hybrid Routing

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VII · target ~4,500 words · 4 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): company/openai/hybrid-sensor-routing.md -->

> The core decision: what runs on-device, what goes to the cloud? Confidence cascading, network-aware routing, cost models, and fleet-scale routing.

## Industry technology (2025–2026)

- **Edge-cloud inference:** Apple Intelligence (on-device for simple tasks, Private Cloud Compute for complex), Google Gemini Nano (on-device) + Gemini Pro (cloud), Samsung Galaxy AI (on-device summarization + cloud translation), Amazon Alexa (local wake word + cloud NLU)
- **Serving frameworks:** vLLM (PagedAttention, continuous batching), SGLang (RadixAttention, prefix caching for fleet), TensorRT-LLM (NVIDIA), Triton Inference Server, BentoML, Ray Serve
- **Confidence/routing:** Speculative execution (draft on device, verify in cloud), confidence thresholds, ensemble disagreement, FrugalGPT (Chen et al., 2023; cascade across model sizes), RouteLLM (2024; learned router for model selection)
- **Fleet-scale systems:** Smart camera platforms (Verkada, Rhombus, custom), drone fleet management, robot fleet inference (Intrinsic/Alphabet), Tesla fleet learning
- **Network-aware:** WebRTC (low-latency media), gRPC (structured inference calls), QUIC (connection migration for mobile), offline-first with CRDT-based sync (for intermittent connectivity)

## What the loops actually ask

- When should inference run on-device vs cloud? Give the decision framework with specific thresholds.
- Design a confidence-based cascade for a 100K smart camera fleet. Each camera runs a small on-device model. When the confidence is below threshold, it escalates to a cloud model. Walk through: model sizes, threshold selection, bandwidth, cost model.
- How do you handle offline and intermittent connectivity in an edge-cloud system? Design the fallback strategy.
- Walk through the cost model: on-device costs power and battery; cloud costs API calls and bandwidth. For a fleet of 100K cameras each making 1 inference/second, compare pure-cloud vs hybrid.
- Design a speculative execution system: the device runs inference optimistically while also sending to the cloud. If the cloud returns first and disagrees, use the cloud result. What are the tradeoffs?
- Explain FrugalGPT / RouteLLM. How do you train a router that decides which model to use?
- A fleet of 10K delivery robots needs real-time obstacle detection. Design the edge-cloud split: what runs on Jetson Orin, what goes to cloud, and how do you handle 200ms network latency?

## Companion code

- `ch22/cascade_router.py` — Confidence-based cascade router: edge-first with cloud escalation, fleet cost analysis (100K cameras example)

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
