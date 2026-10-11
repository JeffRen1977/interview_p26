# Audience and Market

## Primary readers

| Reader | Situation | What they get |
|---|---|---|
| **ML/AI engineers moving into on-device, robotics, or edge roles** | Can train and fine-tune models; weak on sensor pipelines, NPUs, hardware constraints, and real-time systems | Parts II, IV–V: perception, data paths, NPU stack, quantization, HW/SW co-design |
| **Embedded and systems engineers pivoting to AI** | Know DMA, real-time pipelines and memory budgets; new to VLA models, diffusion policies, and LLM inference | Parts III, VI: Physical AI training, embodied AI, VLA, sim-to-real |
| **Robotics engineers who need the full AI stack** | Understand kinematics and control; need the AI model and inference stack underneath | Parts III–VI: training, inference, edge deployment, VLA, autonomous systems |
| **Engineers interviewing for Physical AI roles** | Any background; facing loops at frontier labs, chip vendors, robotics startups, and hyperscalers | Part VIII: coding, system design, AI-enabled coding, behavioral — plus the rest of the book as a structured reference |

## Secondary readers

- **Hiring managers and interviewers** calibrating Physical AI loops. Chapter 2's constraint mindset and level ladder, and each chapter's "what the loops actually ask" section, double as an interviewer's guide.
- **Engineers already in Physical AI roles** who want a structured reference to fill gaps (a common use of interview books once the job is secured).
- **Graduate students** targeting Physical AI roles, as a bridge from coursework to industry expectations.

## Prerequisites

Readers should be comfortable programming in Python. C++ reading ability helps in Parts II, IV–V, and VIII, but every C++ listing has a Python counterpart or explanation. Readers need basic linear algebra (matrix multiplication, dot products) but not prior ML training experience. Appendix B covers the ML fundamentals the rest of the book assumes.

## Not for

Research scientist loops (paper discussions, novel architecture proposals, theory-heavy ML). Classic recommendation, ranking, and cloud-native system design are covered well by existing titles and are not a focus here.

## Market signals

- **Physical AI is the hottest hiring category in AI.** NVIDIA, Tesla, Google, Apple, Qualcomm, Figure AI, Physical Intelligence, and dozens of startups are all hiring engineers who can bridge AI and hardware. Jensen Huang has called Physical AI the next frontier of computing.
- **Demand for AI engineering books is proven.** Chip Huyen's *AI Engineering* (O'Reilly, 2025) was the most-read book on the O'Reilly platform in 2025.
- **No book covers this stack.** Since 2025, several AI interview books have appeared, but they cover cloud-side concerns (LLM serving, RAG, recommendation). None cover sensor pipelines, NPU deployment, hardware-software co-design, VLA models, or sim-to-real — the topics Physical AI loops actually test.
- **C++ is the language of Physical AI.** Most AI books are Python-only. On-device, robotics, and embedded roles interview in C++.

## Author platform and promotion

*To complete with the author. Publishers weigh this heavily. Suggested items:*

- Professional credibility: 17 years shipping ML on consumer devices at Google, Qualcomm, and FutureWei (see `07-author-bio.md`)
- Co-author of *The AI Era CTO* (Springer, forthcoming)
- The companion code repository as a discovery channel (GitHub stars, links from interview-prep and robotics communities)
- Planned promotion: publish selected chapter excerpts and practice projects ahead of launch; post practice projects to Physical AI and interview-prep communities; speak at meetups or conferences on Physical AI engineering and the AI-enabled coding round
- Early-access program (e.g., Manning MEAP), which suits a long book with runnable code
