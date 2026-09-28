# Proposal Package — *From Kernels to Agents*

The publisher-facing proposal. Send these in order, or merge them into one document with Pandoc (below).

| # | File | Contents | Status |
|---|---|---|---|
| 0 | [`00-cover-letter.md`](./00-cover-letter.md) | Query letter to an acquisitions editor | Draft; personalize per publisher |
| 1 | [`01-overview.md`](./01-overview.md) | Title, pitch, why now, differentiators, reader outcomes | Draft |
| 2 | [`02-audience-and-market.md`](./02-audience-and-market.md) | Readers, prerequisites, market signals, promotion | Draft; **promotion section needs you** |
| 3 | [`03-competing-titles.md`](./03-competing-titles.md) | Competitor analysis and positioning, with sources | Draft; confirm editions |
| 4 | [`04-annotated-toc.md`](./04-annotated-toc.md) | Every chapter: abstract, questions, length, figures, code | **Generated**: edit `../build/chapters.py`, then run `python3 manuscript/build/gen.py` |
| 5 | [`05-sample-chapters.md`](./05-sample-chapters.md) | Which samples, why, status | Plan; samples not yet written |
| 6 | [`06-specs-and-schedule.md`](./06-specs-and-schedule.md) | Specifications, schedule, risks, originality statement | Draft |
| 7 | [`07-author-bio.md`](./07-author-bio.md) | Author bio | **Skeleton: needs your facts** |

## Before submitting

- [ ] Fill every [FILL] and [VERIFY] in `07-author-bio.md`; clear all metrics for public use
- [ ] Complete the author platform and promotion section in `02-audience-and-market.md`
- [ ] Confirm competitor editions; skim *Generative AI System Design Interview* and *Inside the AI Systems Interview*
- [ ] Finish at least one sample chapter (Ch 30 recommended first) to final quality
- [ ] Choose target publishers and read each one's proposal guidelines; some have their own template, into which these files map directly
- [ ] Have one outside technical reader review the overview and the sample

## Target publishers

| Publisher | Fit | Notes |
|---|---|---|
| **Manning** | Strong | Hands-on, code-first style; the MEAP early-access program suits a long book delivered in thirds. Publishes Raschka's *Build a Large Language Model (From Scratch)*, which is adjacent, not competing |
| **O'Reilly** | Strong | Large AI engineering readership (*AI Engineering*). *Hands-On LLM Serving and Optimization* is adjacent; position this book as the interview-structured complement |
| **No Starch Press** | Good | Strong editing, practical tone; a smaller AI list could mean more attention |
| **Pragmatic Bookshelf / Apress** | Possible | Faster process; smaller reach in this category |

Submit to one or two at a time, in priority order, unless a publisher explicitly allows simultaneous submissions.

## Build a single proposal document

```bash
cd manuscript/proposal
pandoc 00-cover-letter.md 01-overview.md 02-audience-and-market.md 03-competing-titles.md \
       04-annotated-toc.md 05-sample-chapters.md 06-specs-and-schedule.md 07-author-bio.md \
       -o ../build/proposal.docx
```
