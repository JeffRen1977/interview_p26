# Chapter 24. Agents That Debug the Physical World

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VII · target ~4,500 words · 4 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): behavior/meta-staff-star.md (AF triage agent); AI_native_coding/ -->

> Physical-world debugging as an AI problem. Agent architecture, knowledge graphs, modular skills, and grounded diagnosis.

## Industry technology (2025–2026)

- **LLM agent frameworks:** LangChain / LangGraph (stateful agent graphs, tool use), CrewAI (multi-agent), Autogen (Microsoft, multi-agent), Claude tool use (Anthropic), OpenAI Assistants API (function calling + code interpreter)
- **Knowledge graph tools:** Neo4j (graph database), NetworkX (Python), LlamaIndex Knowledge Graphs, property graphs for failure mode taxonomies
- **Observability for Physical AI:** Rerun.io (multimodal data visualization for robotics — point clouds, images, time series), Foxglove (ROS 2 visualization, web-based), Weights & Biases (experiment tracking), Grafana + Prometheus (time-series)
- **Diagnostic agents in production:** Tesla fleet diagnostics (OTA log collection, automated root-cause), Waymo incident analysis (multi-sensor replay), Google Pixel camera quality agents, industrial predictive maintenance (Siemens MindSphere, PTC ThingWorx)
- **Tool-augmented reasoning:** ReAct pattern (Yao et al., 2023), Toolformer, code-interpreter agents, function calling with structured outputs
- **Grounding techniques:** Retrieval-Augmented Generation (RAG) over hardware docs, structured output (JSON schema enforcement), constrained decoding, chain-of-thought with tool verification

## What the loops actually ask

- Design a triage agent for a fleet of 1000 delivery robots. The agent processes sensor logs (camera, LiDAR, IMU, motor current), runs simulations to reproduce failures, and routes bugs to the right team. Walk through the architecture.
- How do you ground an LLM agent in domain-specific physics? The agent diagnoses motor failures but the LLM has no motor knowledge. (Hint: RAG over failure mode database, tool use for simulation, constrained outputs)
- What is the role of a knowledge graph in a diagnostic agent? Design the schema for a robot failure knowledge graph (failure modes, symptoms, components, fixes).
- How do you handle hallucination in a domain where wrong diagnoses cause hardware damage? Walk through 3 specific mitigations.
- Design a camera quality triage agent. It ingests nightly test results (MTF, color accuracy, noise metrics), detects regressions, hypothesizes root causes (ISP tuning, sensor lot variation, lens decentering), and creates tickets. What tools does it need?
- Compare ReAct and plan-and-execute agent patterns for hardware debugging. When does each work better?
- You built a diagnostic agent that correctly diagnoses 80% of cases. The team doesn't trust it. How do you improve adoption? (Hint: explainability, confidence scores, human-in-the-loop verification)

## Companion code

- `ch24/diagnostic_agent.py` — Diagnostic agent with tool use: failure DB query, sensor log replay, fleet stats, ticket creation
- `ch24/failure_knowledge_graph.py` — Failure mode knowledge graph (NetworkX): components → failure modes → symptoms/root causes

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
