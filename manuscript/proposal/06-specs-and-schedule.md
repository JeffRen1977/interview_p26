# Manuscript Specifications and Schedule

## Specifications

| Item | Plan |
|---|---|
| Length | ~140,000 words (28 chapters + 3 appendices), ~470 printed pages |
| Chapter length | 3,500–5,500 words; three longer chapters (10, 26, 27) at ~7,500 |
| Figures | ~125 original figures: architecture diagrams, memory layouts, sensor pipelines, robot control loops, constraint analysis charts. Editable sources (SVG / draw.io) delivered with the manuscript |
| Code | Python 3.11+ and C++17. Companion repository under MIT license, with CI that runs every listing on each commit. Listings in the text are 40 lines or fewer, excerpted from tested files |
| Math | Standard LaTeX notation, used sparingly; every formula is followed by a worked number |
| Authoring format | Markdown, one file per chapter; converted with Pandoc to the publisher's format (Word, AsciiDoc or LaTeX) |
| Recurring elements | Each chapter follows the same structure: why interviewers ask, what loops ask, mental model, worked problems, code, in the room, failure modes, exercises |
| Back matter | Numbers-and-formulas sheet (Physical AI specific: power, latency, memory, throughput), ML fundamentals refresher, code index, glossary, index |

## Schedule

Dates assume the proposal is submitted in October 2026 and a contract is signed by early 2027. Durations are at part-time pace alongside a full-time job.

| Milestone | Duration | Target |
|---|---|---|
| Proposal submitted, with sample Ch 27 | 3 weeks | Mid-October 2026 |
| Samples Ch 16 and Ch 10 completed (during publisher review) | 6 weeks | Late November 2026 |
| Contract signed | Publisher dependent | ~January 2027 |
| First third (Parts I–III: Landscape, Sensing, Training) delivered for technical review | 8 weeks after contract | ~March 2027 |
| Second third (Parts IV–VI: Inference, Edge, Embodied AI & Autonomous Systems) delivered | 8 weeks | ~May 2027 |
| Final third (Parts VII–VIII: Agents, Interview Rounds, Appendices) delivered | 6 weeks | ~July 2027 |
| Revisions after technical review | 6 weeks | ~September 2027 |
| Production (copyedit, figures, proofs) | Publisher dependent | — |

Delivering in thirds suits early-access programs (such as Manning MEAP), where chapters are released as they are written.

## Risks and how they are handled

| Risk | Mitigation |
|---|---|
| **The field moves fast**, particularly in embodied AI and VLA models | Chapters teach mechanisms (why constraints matter, how tiling exploits memory hierarchy, what makes sim-to-real hard) and treat current models and tools as examples. Fast-moving specifics are confined to marked sidebars. The companion repo can be updated between editions. |
| **Physical AI is broad** — scope creep risk | The out-of-scope list is fixed: research-scientist loops, recommendation and ranking, cloud-native system design (RAG, rate limiting, streaming backends, sandboxes). |
| **Interview formats change** | The book is organized by skill and role archetype, not by company. Chapters 1–2 are the only parts tied to current loop structure, and they are short. |
| **Interview confidentiality** | No question is reproduced from a confidential process. The material is distilled into patterns that are widely discussed publicly. |
| **Embodied AI chapters require depth beyond author's shipped experience** | Author has extensive preparation notes from interview loops at robotics companies. Chapters 17–20 are framed around interview questions and engineering trade-offs, not novel research. Chapter 21 (Automotive AI) draws directly on the author's current work at Qualcomm. A technical reviewer from a robotics team will review these chapters. |
| **Author schedule** | Samples are completed before the contract. The source notes already cover about two thirds of chapters at Medium or High readiness. |

## Originality

All manuscript prose is written fresh by the author. The author's private preparation notes informed the content, but none are reproduced; some were compiled with AI assistance, and the author will disclose AI-tool use according to the publisher's policy. All figures are original. Any third-party code is either rewritten or used under a compatible license with attribution.
