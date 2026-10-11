# Competing and Complementary Titles

Checked by web search on 2026-09-27; updated 2026-10-11 for Physical AI focus. Before submitting, confirm editions and dates, and skim the closest competitors directly.

## Direct competitors: AI interview preparation

| Title | Author, publisher, year | What it covers | How *Physical AI: From Sensors to Agents* differs |
|---|---|---|---|
| *Generative AI System Design Interview* | Ali Aminian, Hao Sheng; ByteByteGo; Nov 2024; 377 pp | A 7-step framework and 10 generative-AI design problems (Smart Compose, translation, ChatGPT-style assistant, image captioning, RAG, text-to-image) | Their problems are designed at the **model level**: data, model choice, training and evaluation. This book goes into the **physical world**: sensor pipelines, NPU deployment, hardware-software co-design, embodied AI, sim-to-real, and edge-cloud agents. Different domain entirely. |
| *Machine Learning System Design Interview* | Ali Aminian, Alex Xu; ByteByteGo; 2023 | Classic ML system design: search, recommendation, ads, ranking | Different domain. This book deliberately leaves recommendation and ranking to it. |
| *Inside the AI Systems Interview* | Self-published (Amazon); 2026 | Inference and serving, ML design (RAG, ranking), ML/LLM fundamentals; tested Python | **Cloud-focused.** Python-only; no sensor pipelines, NPU, hardware-software co-design, embodied AI, VLA, sim-to-real, autonomous systems, or C++. This book covers the Physical AI stack they don't touch. |

## Adjacent: AI engineering references (not interview-structured)

| Title | Author, publisher, year | Relationship |
|---|---|---|
| *AI Engineering* | Chip Huyen; O'Reilly; 2025 | Building applications on foundation models. Cloud-focused. This book is complementary: Physical AI, constraint-first, interview-structured. |
| *Designing Machine Learning Systems* | Chip Huyen; O'Reilly; 2022 | Production ML with traditional models. Different era and domain. |
| *Hands-On LLM Serving and Optimization* | Chi Wang, Peiheng Hu; O'Reilly | Deep reference on cloud LLM serving. Complementary for readers who want more depth on cloud-side inference after Part IV. Does not cover on-device, embodied, or Physical AI. |
| *Build a Large Language Model (From Scratch)* | Sebastian Raschka; Manning; 2024 | Builds a GPT-style model step by step. Good background for Part IV; different goal. |

## Adjacent: robotics and embodied AI

| Title | Author, publisher, year | Relationship |
|---|---|---|
| *Probabilistic Robotics* | Thrun, Burgard, Fox; MIT Press; 2005 | The classic robotics textbook. Deep on SLAM, Bayesian filtering, and probabilistic control. Does not cover learned models, VLA, or modern AI. This book picks up where classical robotics hands off to learned policies. |
| *Modern Robotics* | Lynch, Park; Cambridge; 2017 | Kinematics, dynamics, and control. Provides the foundation that Chapter 19 summarizes for Physical AI engineers. |

No existing book combines sensor pipelines, Physical AI model training, NPU deployment, hardware-software co-design, embodied AI (VLA, sim-to-real), autonomous systems, and edge-cloud agents in an interview-structured format with C++ and Python code.

## Positioning statement

> *Physical AI: From Sensors to Agents* is the first book to cover the full Physical AI engineering stack — from sensor physics and real-time data paths, through model training and inference, to embodied AI and edge-cloud agents — in an interview-structured format with tested C++ and Python code, at Staff depth.

## Sources

- [Generative AI System Design Interview — Amazon](https://www.amazon.com/Generative-AI-System-Design-Interview/dp/1736049143)
- [Inside the AI Systems Interview — Amazon](https://www.amazon.com/Inside-Systems-Interview-Hands-Inference/dp/B0H4C3X76B)
- [AI Engineering — O'Reilly](https://www.oreilly.com/library/view/ai-engineering/9781098166298/)
- [Hands-On LLM Serving and Optimization — O'Reilly](https://www.oreilly.com/library/view/hands-on-llm-serving/9798341621480/)
- [Build a Large Language Model (From Scratch) — Manning](https://www.manning.com/books/build-a-large-language-model-from-scratch)
