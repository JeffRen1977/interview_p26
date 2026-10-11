# Chapter 21. Edge-Cloud Hybrid Routing

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VII · target ~4,500 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): company/openai/hybrid-sensor-routing.md -->

> The core decision: what runs on-device, what goes to the cloud? Confidence-based cascading: local model scores → threshold → cloud escalation. Network-aware routing: RTT estimation, offline fallback, graceful degradation. Cost model: cloud API cost × call rate vs on-device power × battery life. Fleet-scale routing: 100K cameras, each making independent routing decisions.

## Why interviewers ask this

## What the loops actually ask

- When should inference run on device vs cloud?
- Design a confidence-based cascade for a 100K-camera fleet.
- How do you handle offline and intermittent connectivity?
- Walk through the cost model for edge-cloud routing.

## The mental model

## Worked problems

## Code you can run

<!-- Hybrid routing simulator (Python) -->

## In the room

## Failure modes

## Exercises
