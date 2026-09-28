# Sample Chapters

Three chapters were chosen to show the book's range: a fundamentals chapter with code, a design chapter, and the new-round chapter.

| Chapter | Why this one | Status | Remaining work |
|---|---|---|---|
| **9. KV Cache, FlashAttention, and PagedAttention** | Shows the book's core method: derive, implement in Python and C++, test against a reference, then practice the spoken answer. The topic is the most-asked inference question. | Stub. Working Python/C++ implementations exist in the private notes (`LLM/kv_cache.*`, `flash_attention.*`, `paged_attention.*`) | Write ~7,500 words of new prose; move cleaned code and tests to `companion-code/`; draw 7 figures |
| **21. Rate Limiting and Quota for LLMs** | Shows the model-aware design method in a compact, self-contained problem: why a classic answer fails, the dual RPM+TPM design, the scaling drill, and the follow-ups. | Stub. A design note and a Python reference exist (`company/openai/llm-rate-limiter.md`, `token_rate_limiter.py`) | Write ~4,000 words; strip company framing; add tests; draw 4 figures |
| **30. The AI-Enabled Coding Round** | The most distinctive content in the book, which no competing title covers. | **Drafted in Chinese** (`AI_native_coding/chapter_ai_native_coding.md`, 679 lines), with seven runnable practice projects | Rewrite in English (not a line-by-line translation); replace company-specific framing in the introduction; draw 5 figures |

## Quality bar for samples

Samples are the proposal's strongest evidence, so they are written to final-draft quality, not rough drafts:

- Every listing is an excerpt of a file in `companion-code/` that runs and passes its tests
- Every figure is original, drawn in an editable format
- Every number (memory sizes, latencies, speedups) is either derived in the text or measured, with the measurement setup stated
- A technical reader outside the author's circle has read each sample before submission

## Suggested order

1. **Ch 30 first.** The content already exists, so rewriting it establishes the English voice and style fastest.
2. **Ch 21 next.** It is short, and it sets the design-chapter template that Chapters 19–26 follow.
3. **Ch 9 last.** It is the longest and most figure-heavy, and it benefits from the voice settled in the first two.
