# Audience and Market

## Primary readers

| Reader | Situation | What they get |
|---|---|---|
| **Senior and Staff software engineers moving into AI** | Strong in distributed systems or backend; new to inference, GPUs and model-aware design | Model foundations explained from the engineer's side (Part II); inference and serving (Part III); model-aware design (Part VI) |
| **ML engineers moving toward systems** | Can train and fine-tune models; weak on serving, kernels, memory and concurrency | Parts III–V, concurrency (Ch 29), C++ implementations throughout |
| **Embedded and systems engineers moving to on-device AI** | Know DMA, real-time pipelines and memory budgets; new to NPUs and quantization | Part IV, quantization (Ch 11), accelerator data planes (Ch 16) |
| **Engineers preparing for the AI-enabled coding round** | Any background; facing a new and poorly documented format | Ch 30 and its seven practice projects |

## Secondary readers

- **Hiring managers and interviewers** calibrating AI loops. Chapter 2's level ladder and each chapter's "what the loops actually ask" section double as an interviewer's guide.
- **Engineers already in AI roles** who want a structured reference to fill gaps (a common use of interview books once the job is secured).
- **Graduate students** targeting AI infrastructure roles, as a bridge from coursework to industry expectations.

## Prerequisites

Readers should be comfortable programming in Python. C++ reading ability helps in Parts III–V and VIII, but every C++ listing has a Python counterpart or explanation. Readers need basic linear algebra (matrix multiplication, dot products) but not prior ML training experience. Appendix B covers the ML fundamentals the rest of the book assumes.

## Not for

Research scientist loops (paper discussions, novel architecture proposals, theory-heavy ML). Classic recommendation and ranking system design is covered well by existing titles and is not a focus here.

## Market signals

- **Demand for AI engineering books is proven.** Chip Huyen's *AI Engineering* (O'Reilly, 2025) was the most-read book on the O'Reilly platform in 2025.
- **Interview-specific AI books sell.** ByteByteGo publishes two ML interview titles (*Machine Learning System Design Interview* and *Generative AI System Design Interview*, 2024), which shows readers pay for interview-structured AI content.
- **The gap is visible in the supply.** Since 2025, several self-published titles on AI interviews and inference have appeared, which confirms demand. They are mostly Python-only, stop at the serving layer, and are not professionally edited.

## Author platform and promotion

*To complete with the author. Publishers weigh this heavily. Suggested items:*

- Professional credibility: years building ML systems on-device at scale (see `07-author-bio.md`)
- The companion code repository as a discovery channel (GitHub stars, links from interview-prep communities)
- Talks, blog posts or newsletter; any existing audience size
- Planned promotion: publish selected chapter excerpts and practice projects ahead of launch; post the practice projects to interview-prep communities; speak at meetups or conferences on the AI-enabled coding round
- Early-access program (e.g., Manning MEAP), which suits a long book with runnable code
