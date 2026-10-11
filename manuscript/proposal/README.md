# Proposal Package — *Physical AI: From Sensors to Agents*

The publisher-facing proposal. Send these in order, or merge them into one document with Pandoc (below).

| # | File | Contents | Status |
|---|---|---|---|
| 0 | [`00-cover-letter.md`](./00-cover-letter.md) | Query letter to an acquisitions editor | Draft; personalize per publisher |
| 1 | [`01-overview.md`](./01-overview.md) | Title, pitch, why now, differentiators, reader outcomes | Draft |
| 2 | [`02-audience-and-market.md`](./02-audience-and-market.md) | Readers, prerequisites, market signals, promotion | Draft; **promotion section needs you** |
| 3 | [`03-competing-titles.md`](./03-competing-titles.md) | Competitor analysis and positioning, with sources | Draft; confirm editions |
| 4 | [`04-annotated-toc.md`](./04-annotated-toc.md) | Every chapter: abstract, questions, length, figures, code | Draft |
| 5 | [`05-sample-chapters.md`](./05-sample-chapters.md) | Which samples, why, status | Plan; samples not yet written |
| 6 | [`06-specs-and-schedule.md`](./06-specs-and-schedule.md) | Specifications, schedule, risks, originality statement | Draft |
| 7 | [`07-author-bio.md`](./07-author-bio.md) | Author bio | **Has [VERIFY] items to check** |

## Before submitting

- [ ] Fill every [FILL] and [VERIFY] in `07-author-bio.md`; clear all metrics for public use
- [ ] Complete the author platform and promotion section in `02-audience-and-market.md`
- [ ] Confirm competitor editions; skim *Inside the AI Systems Interview* directly
- [ ] Finish at least one sample chapter (Ch 26 recommended first) to final quality
- [ ] Choose target publishers and read each one's proposal guidelines
- [ ] Have one outside technical reader review the overview and the sample

## Target publishers

| Publisher | Fit | Notes |
|---|---|---|
| **Manning** | Strong | Hands-on, code-first style; the MEAP early-access program suits a long book delivered in thirds. No existing Physical AI title |
| **O'Reilly** | Strong | Large AI engineering readership. Would be the first Physical AI engineering title; complements *AI Engineering* and *Hands-On LLM Serving* |
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
