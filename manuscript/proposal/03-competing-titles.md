# Competing and Complementary Titles

Checked by web search on 2026-09-27. Before submitting, confirm editions and dates, and skim the closest competitors (the first three below) directly.

## Direct competitors: AI interview preparation

| Title | Author, publisher, year | What it covers | How *From Kernels to Agents* differs |
|---|---|---|---|
| *Generative AI System Design Interview* | Ali Aminian, Hao Sheng; ByteByteGo; Nov 2024; 377 pp | A 7-step framework and 10 generative-AI design problems (e.g., Smart Compose, translation, a ChatGPT-style assistant, image captioning, RAG, text-to-image), with 280+ diagrams | Their problems are designed at the **model level**: data, model choice, training and evaluation. This book goes **below and around the model**: inference internals, serving, on-device and accelerator software, rate limiting, sandboxes and agents. It also covers coding, concurrency, the AI-enabled coding round and behavioral. The two are complementary. |
| *Machine Learning System Design Interview* | Ali Aminian, Alex Xu; ByteByteGo; 2023 | Classic ML system design: search, recommendation, ads, ranking | Different domain. This book deliberately leaves recommendation and ranking to it. |
| *Inside the AI Systems Interview: A Hands-On Guide to Machine Learning Systems Design, Model Serving, and LLM Inference — with Tested Python* | Self-published (Amazon); 2026 | Inference and serving (batching, backpressure, streaming, cancellation, KV cache, rate limiting), ML design (RAG, ranking, A/B testing), ML/LLM fundamentals; tested Python | **The closest in intent.** Based on its published description: Python-only; no kernels, quantization internals, on-device/NPU, accelerator data planes, compilers or training infrastructure; no AI-enabled coding round; no explicit level framing. This book adds C++, the full hardware-to-agent stack, and Staff-level answer structure. Its title is also why this proposal avoids "AI Systems Interview". |

## Adjacent: AI engineering references (not interview-structured)

| Title | Author, publisher, year | Relationship |
|---|---|---|
| *AI Engineering: Building Applications with Foundation Models* | Chip Huyen; O'Reilly; 2025 | Covers building applications on foundation models (prompting, RAG, fine-tuning, evaluation). It is the market-demand signal. This book is complementary: interview-structured, and deeper on inference and systems. |
| *Designing Machine Learning Systems* | Chip Huyen; O'Reilly; 2022 | Covers production ML with traditional models. Different era and focus. |
| *Hands-On LLM Serving and Optimization* | Chi Wang, Peiheng Hu; O'Reilly | A deep reference on serving (speculative decoding, parallelism, KV caching, vLLM/TensorRT-LLM/SGLang internals). Readers who want more depth after Part III go here. It is not organized around interviews and doesn't cover on-device, product design or the rounds. |
| *LLM Engineer's Handbook* | Paul Iusztin, Maxime Labonne; Packt; 2024 | Covers the LLMOps lifecycle through one end-to-end project. Its orientation is building rather than interviewing. |
| *Build a Large Language Model (From Scratch)* | Sebastian Raschka; Manning; 2024 | Builds a GPT-style model step by step. Good background for Part II; different goal. |
| *Hands-On Large Language Models* | Jay Alammar, Maarten Grootendorst; O'Reilly; 2024 | Covers using and fine-tuning LLMs, with a visual approach. Different goal. |

## Adjacent: general interview preparation

*System Design Interview* (Alex Xu, vols. 1–2) and *Cracking the Coding Interview* (Gayle Laakmann McDowell) define the genre and its readers' expectations: a repeatable framework, worked problems, and honest guidance on what interviewers score. Neither covers AI-specific material.

## Positioning statement

> *From Kernels to Agents* is the only interview book that covers the full AI engineering stack, from GPU kernels, quantization and accelerator software up through serving, agents and evaluation. It has tested code in both Python and C++, Staff-level answer structure, and a full treatment of the new AI-enabled coding round.

## Sources

- [Generative AI System Design Interview — Amazon](https://www.amazon.com/Generative-AI-System-Design-Interview/dp/1736049143)
- [Generative AI System Design Interview — review (Javarevisited)](https://javarevisited.wordpress.com/2025/10/21/review-is-generative-ai-system-design-interview-book-by-bytebytego-worth-it-2/)
- [Inside the AI Systems Interview — Amazon](https://www.amazon.com/Inside-Systems-Interview-Hands-Inference/dp/B0H4C3X76B)
- [Inside the AI Systems Interview — clcoding summary](https://www.clcoding.com/2026/06/inside-ai-systems-interview-hands-on.html)
- [AI Engineering — O'Reilly](https://www.oreilly.com/library/view/ai-engineering/9781098166298/)
- [Chip Huyen — books](https://huyenchip.com/books/)
- [Hands-On LLM Serving and Optimization — O'Reilly](https://www.oreilly.com/library/view/hands-on-llm-serving/9798341621480/)
- [LLM Engineer's Handbook — Amazon](https://www.amazon.com/LLM-Engineers-Handbook-engineering-production/dp/1836200072)
- [Build a Large Language Model (From Scratch) — Manning](https://www.manning.com/books/build-a-large-language-model-from-scratch)
