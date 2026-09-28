# Book Plan — *From Kernels to Agents*

*A field guide to engineering interviews for AI roles: inference, training infra, on-device AI, LLM product systems, embodied AI, and the new AI-enabled coding round.*

> Internal planning document. The publisher-facing proposal is in [`manuscript/proposal/`](./manuscript/proposal/). Chapter structure is maintained in [`manuscript/build/chapters.py`](./manuscript/build/chapters.py); if the two disagree, `chapters.py` wins.

Status: plan · Drafted 2026-09-27 · Language: English · Format: manuscript for a trade/technical publisher · Source corpus: this repository

---

## 1. The pitch in one paragraph

Most AI interview books are written for ML *researchers* (derive backprop, explain bias–variance) or for generic SWE loops (LeetCode + a URL shortener). The market has moved: the bulk of AI hiring in 2025–2026 is for **engineers who make models run**: they serve them, shrink them onto phones and wearables, keep GPUs fed, wrap them in products, and increasingly write code *with* them. Those loops ask questions that neither kind of book covers: *why is decode memory-bound, design an RPM+TPM rate limiter, run a multimodal model on a 1 W budget, fix this unfamiliar repo in 60 minutes with an AI assistant.* This book is organized around those questions. Every chapter is backed by runnable code and by answers rehearsed against real interview loops at frontier labs, hyperscalers, GPU vendors and mobile-chip vendors. That experience is distilled into patterns, not organized by company.

## 2. Audience

| Primary reader | What they want from the book |
|---|---|
| Senior / Staff / Principal SWE moving into AI infra or AI product teams | A map of what is actually asked, and model answers at Staff depth |
| ML engineers who can train models but are weak on serving, systems, and edge | The systems half: KV cache, batching, quantization, NPU, concurrency |
| Embedded / systems engineers pivoting to on-device AI | A bridge from what they already know (DMA, real-time pipelines, memory budgets) to AI workloads |
| Anyone facing the AI-enabled coding round | A method, not tips — and practice repos that are red on day one |

**Not** the audience: pure research scientist loops (paper reading, novel architecture proposals). The proposal leans deliberately into the corpus's real strength — systems, infra, and edge.

## 3. What makes it different

1. **Runnable evidence.** The repo already contains paired Python/C++ implementations (KV cache, FlashAttention, PagedAttention, GEMM, concurrent queues, rate limiters, sandbox orchestrator, voice pipeline) and seven AI-native coding practice projects with tests. The book cites them; readers run them.
2. **Staff-level framing.** Every design chapter ends with a "10x / 100x scaling drill", a 3-minute spoken version, and the follow-up questions the interviewer actually asks next.
3. **The "remaining skills" thesis.** The AI-native coding chapter already argues that when writing code is cheap, the scarce skills are *specifying* and *verifying*. That thesis becomes the spine of the whole book: each part ends by asking what an interviewer can still learn about you when an AI can produce the textbook answer.
4. **Edge + cloud in one book.** Few resources treat mobile-NPU power budgets and 100M-user streaming backends as the same discipline. This corpus does.
5. **Patterns, not company folklore.** Interview experience from many companies is folded into the chapters as question patterns and scoring criteria, so the book stays useful when a specific company changes its loop.

## 4. Scope decisions

**Language: English.** Chinese source files are research notes: translate and restructure them, don't transliterate them. Keep standard English technical terms (prefill, decode, KV cache, MFU).

**Camera / XR content is excluded.** Nothing from `camera/`, `XR/` (including `XR/Pico_vision/` and `XR/Meta_Camera/`) is used as a source. That removes the former "Vision for AI products" chapter (CV fundamentals, AI-ISP, hand/eye/face tracking, MR perception). Two knock-on effects:

- The **concurrency chapter** is drafted with camera-pipeline examples (frame dropping, video ring buffer, SD-card writer, camera config); about 27 references. They need reframing onto AI workloads: request queues, token streams, latest-state caches, model/config hot-reload.
- The **behavioral story library** (`behavior/meta-staff-*.md`) is built on camera stories: autofocus agent, a DXO regression, an LSTM disagreement in AF. Chapter 31 keeps the *method* (theme library, STAR structure, delivery timing). Its worked examples need to be AI-domain stories (see Section 9).

**Company-specific content is abstracted into chapters.** No company playbook appendix and no company-named chapters. Section 7 gives the rules and the source-to-chapter mapping.

**Format: a manuscript for a publisher.** The deliverable is a full manuscript plus a companion code repository, not a website. Section 10 covers the proposal package, the manuscript specifications, and the originality requirements that this implies.

## 5. Table of contents

Eight parts, 31 chapters, 3 appendices, ~147,500 words (~490 pages). The full annotated TOC is [`manuscript/proposal/04-annotated-toc.md`](./manuscript/proposal/04-annotated-toc.md), generated from `manuscript/build/chapters.py`.

| Part | Chapters |
|---|---|
| I. The Landscape | 1 Job map · 2 How loops are scored |
| II. Model Foundations | 3 LLM anatomy · 4 MoE · 5 Decoding · 6 Pre-training · 7 Post-training |
| III. Inference Systems | 8 Prefill/decode · 9 KV/Flash/Paged ★ · 10 Kernels & GPU · 11 Quantization · 12 Serving |
| IV. On-Device and Edge AI | 13 NPU stack · 14 LLMs on a phone · 15 Wearables |
| V. AI Infrastructure | 16 Data planes · 17 ML compilers · 18 Overlap |
| VI. AI Product System Design | 19 Model-aware design · 20 Streaming · 21 Rate limiting ★ · 22 RAG · 23 Agents · 24 Evals · 25 Sandbox · 26 Real-time multimodal |
| VII. Embodied AI | 27 Embodied & VLA |
| VIII. The Rounds | 28 Coding · 29 Concurrency · 30 AI-enabled coding ★ · 31 Behavioral |
| Appendices | A Numbers & formulas · B ML fundamentals refresher · C Code index |

★ = sample chapter. Compared with the first draft of this plan, three gap topics became content: LLM evaluation (Ch 24), agents (Ch 23) and GPU basics (folded into Ch 10). ML fundamentals became Appendix B. Recommendation/ranking is out of scope, since existing titles cover it.

## 6. Standard chapter template

Modeled on the two chapters already drafted (`AI_native_coding/chapter_ai_native_coding.md`, `concurrency/CHAPTER_CONCURRENCY.md`):

1. **Why interviewers ask this** — the signal being tested, by level
2. **What the loops actually ask** — question patterns distilled from company tracks, labeled by role archetype ("inference-platform teams", "on-device SDK teams"), never by company
3. **The mental model** — one diagram, one table, the core equations
4. **Worked problems** — 3–6 real questions, each with a Staff-depth answer
5. **Code you can run** — short listings in the text (≤40 lines each); the full code lives in the companion repo
6. **In the room** — the 60-second and 3-minute spoken versions; follow-ups and how to answer
7. **Failure modes** — what weak answers look like
8. **Exercises** — with answer sketches

## 7. Abstracting company-specific content

### Rules

| Keep (as generic content) | Rewrite | Drop |
|---|---|---|
| Question patterns and the follow-ups that come after them | Company framing → role archetype ("OpenAI asks…" → "frontier-lab product infra loops ask…") | Company names as chapter or section framing |
| Answer frameworks (TLM 45-min flow, scaling drill, no-buzzword rule) | Proprietary product stacks → the general concept, with public products as one example (QNN/Hexagon → mobile NPU SDK; Nitro/Trainium/Neuron → host offload, accelerator, ML compiler) | "Why company X", company-culture keyword lists, questions to ask a specific interviewer |
| Scoring criteria by level (Senior / Staff / Principal) | Company-specific leadership principles → the underlying behavioral signal they test | Material from internal prep documents; team names; loop logistics |
| Technical content and code | Company-specific numbers → reasoned estimates the reader can re-derive | Company-specific prep calendars (fold the generic parts into Ch 2) |

### Where each company track goes

| Source track | Feeds chapters | Main abstraction |
|---|---|---|
| `company/openai/` — TLM playbook, 4-week plan, 10 system designs, Python/C++ demos | 1, 2, 15, 19–23, 25, 26, 28 | TLM framework → Ch 19's model-aware design method; each design → a generic problem statement |
| `company/microsoft/` — Principal ML systems, GEMM | 2, 6, 10, 12, 31 | Principal-level signals → Ch 2 level ladder; leadership questions → Ch 31 |
| `company/amazon/` — Nitro/Trainium, user-space driver, Neuron compiler, C++ docs | 16, 17, 18, 28 | Offload card + accelerator → generic host/accelerator data plane; Neuron → generic ML compiler pipeline |
| `company/nvidia/` — system design, C++, performance, behavioral, RAG and ingestion designs | 12, 16, 22, 28, 31 | GPU scheduler → Ch 12; RAG/ingestion → Ch 22; "One Team" → disagreement and collaboration signals in Ch 31. Drop autonomous-driving domain questions (Hybrid A*) |
| `AI_edge/` — Qualcomm AI Stack, on-device LLM, Senior Staff guide | 11, 13, 14 | Vendor SDK → generic NPU stack chapter; RCA cases kept as anonymized debugging stories |
| `AI_native_coding/` — Meta AI-enabled coding round | 30 | "Meta piloted this in 2025" → "large companies began piloting this format in 2025"; practice projects stay as they are |

## 8. Source map and readiness

Per-chapter sources and readiness live in `manuscript/build/chapters.py`, and each chapter stub in `manuscript/chapters/` carries them in its header comment. Summary: 2 chapters drafted (29, 30), 12 High, 10 Medium, 4 Low, 3 new writing. Readiness was estimated from headings and file lengths, not a full read. Most sources are Chinese notes and need translating into fresh English prose.

## 9. Content gaps

Resolved in the 31-chapter structure: evaluation (Ch 24), agents (Ch 23), GPU programming (Ch 10), ML fundamentals (Appendix B). Recommendation/ranking is out of scope.

Still open: **AI-domain behavioral stories for Ch 31.** With the camera stories excluded, the chapter needs 4–6 stories from your AI work (e.g., the LLM bug-triage agent, the learned-model adoption, on-device ML trade-offs). Only you can supply these.

## 10. Publishing as a manuscript

### 10.1 The proposal package

Technical publishers (O'Reilly, Manning, No Starch, Apress, Pragmatic) all ask for roughly the same package before offering a contract:

| Item | Source in this plan | Status |
|---|---|---|
| Overview and "why now" | Sections 1 and 3 | Drafted |
| Target audience and prerequisites | Section 2 | Drafted |
| Competing titles and how this one differs | New | To write (see 10.2) |
| Author bio and platform | New | Only you can write this |
| Annotated table of contents | Section 5 | Drafted; expand each chapter into a 3–5 sentence abstract |
| Length, figures, schedule | Sections 5 and 12 | Drafted |
| Two or three sample chapters | Ch 9, 21, 30 | Ch 30 drafted (needs English rewrite); Ch 9 and 21 need writing |

Publishers differ in what they want first. Some take the proposal alone, while others want sample chapters up front. Check each publisher's author guidelines before submitting.

### 10.2 Competing titles

Done: see `manuscript/proposal/03-competing-titles.md` (web-checked 2026-09-27). Key finding: a 2026 self-published title, *Inside the AI Systems Interview*, forced the retitle to *From Kernels to Agents*. The original candidate list follows. For each, say what it covers and where this book is different:

- Chip Huyen, *Designing Machine Learning Systems* (O'Reilly) and *AI Engineering* (O'Reilly): production ML and building on foundation models. They are not organized around interviews.
- Ali Aminian and Alex Xu, *Machine Learning System Design Interview* (ByteByteGo): interview-focused, but on classic ML systems (recommendation, search, ads) rather than LLM serving, edge AI or accelerator software.
- ByteByteGo's generative-AI system design interview title: the closest competitor. Describe precisely how this book's focus on inference internals, on-device AI and the AI-enabled coding round differs.
- Generic coding-interview books (e.g., *Cracking the Coding Interview*): nothing AI-specific.

### 10.3 Manuscript specifications

- **Authoring format:** keep writing in Markdown in `manuscript/`, one file per chapter. Convert with Pandoc to whatever the publisher needs (Word, AsciiDoc or LaTeX) once a contract names the toolchain. Avoid features Pandoc can't convert cleanly, such as HTML embeds and GitHub-only callouts.
- **Figures:** plan on about 3–5 original figures per chapter (~110 total), kept as editable source files such as SVG or draw.io. Every figure has to be drawn fresh; don't reuse diagrams from papers, vendor docs or blog posts without permission.
- **Math:** standard LaTeX math only.
- **Code:** a companion repo with an MIT or Apache-2.0 license, tagged to match the printed book, with CI that runs every listing. Listings in the text are excerpts from files that compile and pass tests.

### 10.4 Originality and rights

This is the main risk in moving from prep notes to a published book:

- **Assistant-generated notes.** Several source files read like chat-assistant output. `LLM/MoE.md`, `LLM/矩阵乘法.md` and `LLM/量化计算.md`, for example, open by addressing "you" with "here is…" / "I've organized…". Treat every source file as research notes and write the manuscript prose fresh, in your own voice. Many publishers now ask authors to disclose AI assistance, so read the contract's policy early and keep a record of how AI tools were used.
- **Third-party text and code.** Check any passage or listing that may come from docs, blogs or papers. Rewrite it, or cite it and seek permission.
- **Interview confidentiality.** Candidates often agree not to share interview questions. The patterns-not-companies approach from Section 7 mostly covers this. Also avoid reproducing any specific question word for word if it came from a loop covered by an NDA, and exclude anything from internal prep documents entirely.
- **Employer IP.** Anonymize any debugging stories or behavioral examples drawn from your employers' products, and check your employment agreement for side-publication clauses.

## 11. Repo housekeeping before drafting

Found during the review; worth fixing so chapters can link reliably:

- Root `README.md` links to `./docs/`, `./openai/`, `./nvidia/`, `./microsoft/` — none of these exist anymore (content moved to `XR/Pico_vision/`, `AI_edge/`, `embodied_AI/`, `company/*`). `XR/README.md`, `behavior/README.md`, and `embodied_AI/README.md` have the same stale paths.
- `company/README.md` references `../AI端侧/`, which is now `AI_edge/`.
- `AI_native_coding/` has both `maze_solver_c++/` and `maze_solver_cpp/`.
- `git status` shows `XR/Meta_Camera/AI_native_coding/` deleted (moved to top-level `AI_native_coding/`), which is not committed yet.
- `build/` directories are present in several folders; they should be in `.gitignore`.
- Doc-number prefixes (`16-`, `19-`, `27-`, …) come from the old flat `docs/` numbering and no longer mean anything; the book's chapter numbers should replace them.

## 12. Plan and milestones

| Phase | Duration | Output |
|---|---|---|
| 0. Proposal package | 3 weeks | Annotated TOC, competing titles, bio, `manuscript/` skeleton; sample chapters 9, 21, 30 written to final quality |
| — | (publisher review, 1–3 months) | Contract; toolchain and house style confirmed |
| 0b. Housekeeping | 1 week | Links fixed, companion-repo layout, Pandoc build working |
| 1. High-readiness chapters | 6 weeks | Ch 8–10, 14, 15, 18, 20–22, 25, 26, 28–30: restructure into template, translate, de-company, reframe concurrency examples |
| 2. Medium-readiness chapters | 6 weeks | Ch 3, 6, 7, 11–13, 16, 17, 19, 27 |
| 3. New writing | 5 weeks | Ch 1–2, 4–5, 23, 24, 31, Appendix B |
| 4. Appendices + code index | 1 week | A–B; CI that runs every code sample |
| 5. Author review pass | 2 weeks | Consistency pass, de-company and originality audit, mock-interview test with 2–3 readers |
| 6. Publisher cycle | set by publisher | Technical review → revisions → copyedit → figure redraw → proofs |

About **3 weeks to a submittable proposal**, then about **21 weeks** of part-time writing after the contract to deliver the full manuscript. Sample chapters: Chapter 9 (KV/Flash/Paged), Chapter 21 (rate limiting) and Chapter 30 (AI-enabled coding). Detailed schedule: `manuscript/proposal/06-specs-and-schedule.md`. Together they show the range: fundamentals, design, and the new round.

## 13. Proposed repository layout

```
manuscript/
├── 00-front-matter.md     # preface, how to use this book
├── 01-job-map.md … 29-behavioral.md
├── appendix-a-numbers.md
├── appendix-b-code-index.md
├── figures/               # editable sources (SVG / draw.io) + exported PNG/PDF
├── proposal/              # publisher proposal: overview, annotated TOC, comps, bio
└── build/                 # Pandoc scripts → .docx / AsciiDoc / PDF
companion-code/            # becomes the public companion repo; excerpts from LLM/, concurrency/, AI_native_coding/, company/openai/*.py, cleaned and tested
```

Existing folders stay as private notes and aren't published. Only `manuscript/` and `companion-code/` leave this repo. `camera/` and `XR/` aren't referenced from either.
