# Sample Chapters

Three chapters were chosen to show the book's range: an inference chapter with code, a Physical AI design chapter, and the AI-enabled coding round chapter.

| Chapter | Why this one | Status | Remaining work |
|---|---|---|---|
| **10. Attention, KV Cache, and Memory** | Shows the book's core method: derive, implement in Python and C++, test against a reference, then practice the spoken answer. The KV cache is the most-asked inference question, and it matters for both cloud and on-device deployment. | Stub. Working Python/C++ implementations exist in the private notes (`LLM/kv_cache.*`, `flash_attention.*`, `paged_attention.*`) | Write ~7,500 words of new prose; move cleaned code and tests to `companion-code/`; draw 7 figures |
| **16. Hardware-Software Co-Design** | The most distinctive content in the book. No competing title covers HW/SW co-design from the perspective of someone who shipped it. Shows the constraint-first mindset that defines Physical AI. | Stub. Based on author's direct experience (PDAF case study, 47% area reduction). | Write ~4,500 words of new prose; anonymize the case study; draw 4 figures |
| **27. The AI-Enabled Coding Round** | The newest interview format, which no competing title covers. | **Drafted in Chinese** (`AI_native_coding/chapter_ai_native_coding.md`, 679 lines), with seven runnable practice projects | Rewrite in English (not a line-by-line translation); replace company-specific framing; draw 5 figures |

## Quality bar for samples

Samples are the proposal's strongest evidence, so they are written to final-draft quality, not rough drafts:

- Every listing is an excerpt of a file in `companion-code/` that runs and passes its tests
- Every figure is original, drawn in an editable format
- Every number (memory sizes, latencies, speedups) is either derived in the text or measured, with the measurement setup stated
- A technical reader outside the author's circle has read each sample before submission

## Suggested order

1. **Ch 27 first.** The content already exists, so rewriting it establishes the English voice and style fastest.
2. **Ch 16 next.** It is the book's most unique chapter and demonstrates the constraint-first approach that defines the book's voice.
3. **Ch 10 last.** It is the longest and most figure-heavy, and it benefits from the voice settled in the first two.
