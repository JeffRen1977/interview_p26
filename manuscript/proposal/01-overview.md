# Book Proposal: *From Kernels to Agents*

**Subtitle:** The Engineering Interview for AI Roles
**Author:** Jeff Ren
**Format:** Print and ebook, ~147,000 words (~490 pages), ~130 original figures, companion code repository (Python + C++, MIT license)
**Status:** Proposal with annotated table of contents; sample chapters in progress

---

## Title

*From Kernels to Agents* describes the range the book covers: from GEMM kernels and KV-cache layouts at the bottom of the stack up to agents, retrieval and evaluation at the top. The subtitle says who it is for.

Alternates, if the publisher prefers a more literal title:

- *Making Models Run: Interviewing for AI Engineering Roles*
- *The AI Engineering Interview: Inference, Infrastructure, On-Device, and Agents*

Avoid "AI Systems Interview" in the title: a 2026 self-published book uses *Inside the AI Systems Interview* (see `03-competing-titles.md`).

## The book in one paragraph

Most AI hiring today is not for researchers. It is for engineers who make models run: they serve them to millions of users, shrink them onto phones, keep accelerators fed, and build products and agents around them. The interview loops for these roles have changed faster than the books that prepare people for them. Generic system design has become model-aware system design, where tokens per second and GPU memory are the constraints. Domain rounds probe why decode is memory-bound or how a mobile NPU runs a quantized graph. Some companies have replaced an algorithm round with an AI-enabled coding round, where the candidate works in an unfamiliar codebase with an AI assistant. *From Kernels to Agents* prepares engineers for all of it. It is organized around the questions these loops actually ask, answered at Staff depth, and backed by runnable Python and C++ code that readers can execute, modify and test.

## Why now

- **The job market has moved to AI engineering.** Hiring has shifted from model research toward inference, infrastructure, on-device deployment and AI products. Chip Huyen's *AI Engineering* was the most-read book on the O'Reilly platform in 2025, which shows how large the engineering audience for AI has become.
- **The loops have changed, and preparation material has not caught up.** Existing interview books cover classic ML system design (recommendation, search) or generative-AI product design at the model level. None cover the systems depth that inference, on-device and accelerator-software teams test. None cover the AI-enabled coding round.
- **The supply of preparation material is noisy.** Self-published titles and blog posts have multiplied since 2025, but they are mostly Python-only, rarely tested, and not organized around the levels at which candidates are evaluated. A carefully edited book from an established publisher has room to become the standard reference.

## What makes this book different

1. **Full-stack range.** One book covers model foundations, inference internals, on-device and edge AI, accelerator software, AI product system design, embodied AI, and the individual interview rounds. Readers targeting any AI engineering archetype find their loop covered, and see how the layers connect.
2. **Runnable, tested code in two languages.** Core techniques (KV cache, FlashAttention, PagedAttention, GEMM optimization, quantization, lock-free queues, rate limiters, sandboxes, streaming servers) are implemented in Python and C++ and tested against naive reference implementations. Systems and on-device roles interview in C++, and most competing books do not.
3. **Staff-level answers.** Each design chapter includes the answer flow, the 10x/100x scaling drill, the follow-ups interviewers ask next, and 60-second and 3-minute spoken versions. Chapter 2 makes the Senior/Staff/Principal bar explicit.
4. **The AI-enabled coding round.** The book has a full chapter on the newest interview format, with seven practice projects that start with failing tests. Its thesis runs through the whole book: when code is cheap, the scarce skills are specifying and verifying.
5. **Patterns, not company folklore.** Interview experience across frontier labs, hyperscalers, GPU and mobile-chip vendors, and consumer-internet companies is distilled into question patterns by role archetype. The book stays accurate when an individual company changes its process.

## What the reader will be able to do

- Identify which AI engineering archetype fits them, and prepare efficiently for its loop
- Estimate FLOPs, memory, bandwidth and latency for a model on given hardware, on a whiteboard
- Implement attention, the KV cache, FlashAttention-style tiling, top-p sampling and INT8 quantization from scratch, in Python or C++
- Design LLM serving, streaming, rate limiting, RAG, agents, evaluation and code sandboxes at Staff depth
- Explain how a model reaches a mobile NPU or a datacenter accelerator, and debug where it goes wrong
- Work effectively and verifiably in an AI-enabled coding round
- Deliver behavioral answers that show Staff-level scope

## Structure

Eight parts, 31 chapters, three appendices. See `04-annotated-toc.md`.

| Part | Chapters | Focus |
|---|---|---|
| I. The Landscape | 1–2 | Role archetypes; how loops are scored |
| II. Model Foundations | 3–7 | Architecture, MoE, decoding, pre-training, post-training |
| III. Inference Systems | 8–12 | Prefill/decode, KV cache and attention, kernels, quantization, serving |
| IV. On-Device and Edge AI | 13–15 | NPU stack, LLMs on phones, wearables |
| V. AI Infrastructure | 16–18 | Accelerator data planes, ML compilers, compute–communication overlap |
| VI. AI Product System Design | 19–26 | Model-aware design, streaming, rate limiting, RAG, agents, evals, sandboxes, real-time multimodal |
| VII. Embodied AI | 27 | VLA models, sim-to-real, robot data |
| VIII. The Rounds | 28–31 | Coding, concurrency, AI-enabled coding, behavioral |
