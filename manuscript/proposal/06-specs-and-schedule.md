# Manuscript Specifications and Schedule

## Specifications

| Item | Plan |
|---|---|
| Length | ~147,500 words (31 chapters + 3 appendices), ~490 printed pages |
| Chapter length | 3,500–6,000 words; three longer chapters (9, 29, 30) at ~7,500 |
| Figures | ~130 original figures: architecture diagrams, memory layouts, timelines, rooflines. Editable sources (SVG / draw.io) delivered with the manuscript |
| Code | Python 3.11+ and C++17. Companion repository under MIT license, with CI that runs every listing on each commit. Listings in the text are 40 lines or fewer, excerpted from tested files |
| Math | Standard LaTeX notation, used sparingly; every formula is followed by a worked number |
| Authoring format | Markdown, one file per chapter; converted with Pandoc to the publisher's format (Word, AsciiDoc or LaTeX) |
| Recurring elements | Each chapter follows the same structure: why interviewers ask, what loops ask, mental model, worked problems, code, in the room, failure modes, exercises |
| Back matter | Numbers-and-formulas sheet, ML fundamentals refresher, code index, glossary, index |

## Schedule

Dates assume the proposal is submitted in October 2026 and a contract is signed by early 2027. Durations are at part-time pace alongside a full-time job.

| Milestone | Duration | Target |
|---|---|---|
| Proposal submitted, with sample Ch 30 | 3 weeks | Mid-October 2026 |
| Samples Ch 21 and Ch 9 completed (during publisher review) | 6 weeks | Late November 2026 |
| Contract signed | Publisher dependent | ~January 2027 |
| First third (Parts I–III) delivered for technical review | 8 weeks after contract | ~March 2027 |
| Second third (Parts IV–VI) delivered | 8 weeks | ~May 2027 |
| Final third (Parts VII–VIII, appendices) delivered | 6 weeks | ~July 2027 |
| Revisions after technical review | 6 weeks | ~September 2027 |
| Production (copyedit, figures, proofs) | Publisher dependent | — |

Delivering in thirds suits early-access programs (such as Manning MEAP), where chapters are released as they are written.

## Risks and how they are handled

| Risk | Mitigation |
|---|---|
| **The field moves fast**, and specific models and tools date quickly | Chapters teach mechanisms (why decode is memory-bound, why paging fixes fragmentation) and treat current models and tools as examples. Fast-moving specifics are confined to marked sidebars that are easy to update in later editions. The companion repo can be updated between editions. |
| **Interview formats change** | The book is organized by skill and role archetype, not by company. Chapters 1–2 are the only parts tied to current loop structure, and they are short. |
| **Interview confidentiality** | No question is reproduced from a confidential process. The material is distilled into patterns that are widely discussed publicly. |
| **Scope creep** (the book tries to cover everything) | The out-of-scope list is fixed: research-scientist loops, recommendation and ranking, computer vision and imaging domain rounds. |
| **Author schedule** | Samples are completed before the contract. The source notes already cover about two thirds of chapters at Medium or High readiness. |

## Originality

All manuscript prose is written fresh by the author. The author's private preparation notes informed the content, but none are reproduced; some were compiled with AI assistance, and the author will disclose AI-tool use according to the publisher's policy. All figures are original. Any third-party code is either rewritten or used under a compatible license with attribution.
