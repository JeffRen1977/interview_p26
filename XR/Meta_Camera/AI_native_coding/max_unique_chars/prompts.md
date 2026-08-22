# Maximize Unique Characters — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read wordlist.py, solver.py and tests/test_max_unique_chars.py.
Do NOT write implementation code.

List:
1. The exact contract of sanitize() as the tests define it
2. Whether max_unique_length is expected to sanitize its own input
3. What the two Phase 3 stress shapes are and what each one is punishing
4. Edge cases: empty input, all-invalid input, duplicate words, non-ASCII
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Three Phase-1 failures:
  test_trims_lowercases_and_drops_invalid_words
  test_rejects_letters_outside_ascii_a_to_z
  test_unique_char_count_counts_distinct_characters

Explain each root cause in one sentence, naming the exact line in wordlist.py.
Also tell me what breaks downstream if an uppercase letter survives sanitize,
given that solver.word_mask uses ord(c) - ord('a').
Do NOT propose code yet.
```

---

## Prompt 2 — Phase 1 修复

```
Task: fix sanitize and unique_char_count in wordlist.py.
Constraints:
- sanitize: strip, lower-case, keep only words that are non-empty, ASCII a-z,
  and free of repeated characters; preserve order; keep duplicate candidates
- unique_char_count returns the number of DISTINCT characters
- Signatures unchanged; do not touch solver.py
Return only the two function bodies.
```

跑：`python3 -m unittest discover -s tests -k Phase1 -v`

---

## Prompt 3 — Phase 2 回溯

```
Task: implement max_unique_length(words) -> int in solver.py.
Constraints:
- Call sanitize() on the raw input first
- Plain take/skip backtracking; a word may be taken only if none of its letters
  are already used
- Return 0 when no candidate survives sanitize
- Do NOT sort by length and greedily take the longest first — that is wrong:
  ["abcd", "ab", "cef"] must return 5, not 4
- Do NOT touch max_unique_length_fast or wordlist.py
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase2 -v`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste unittest output>
```

---

## Prompt 5 — Phase 3（先说瓶颈，再让它写）

```
The stress set is every 2-letter word over a 12-letter alphabet: 66 candidates,
so 2^66 subsets. But two subsets covering the same letters are the same state,
and there are at most 2^12 reachable letter sets here.

Task: implement max_unique_length_fast(words) -> int as DP over reachable
26-bit masks.
Constraints:
- Deduplicate words by mask first, so 10,000 copies of "abc" cost one mask
- reachable = {0}; for each word mask, extend every compatible state with |,
  test compatibility with `state & mask` (O(1), no set operations)
- Track the best popcount as you go; return 0 for an empty candidate list
- Must return exactly the same value as max_unique_length; keep that function
  untouched so I can differential-test them
- Use the given word_mask/popcount helpers; do not add dependencies
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase3 -v`

---

## Prompt 6 — 复杂度确认

```
State the complexity of max_unique_length_fast in terms of #words and #distinct
reachable masks, and tell me which input shape makes the reachable set blow up.
Do NOT change code.
```

（期望答案：O(W × R)；当字母表接近 26 且词两两高度兼容时 R 趋近 2²⁶ —— 那时才需要按 popcount 排序 + 上界剪枝。）

---

## Prompt 7 — 收尾 review

```
Review solver.py and wordlist.py against the tests only.
Call out dead code, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "I considered a Trie here and decided against it: the bottleneck is the subset
> combination, not shared prefixes. A Trie would pay off if the task were
> segmenting a long text into non-overlapping dictionary words — different
> problem shape. Bitmask DP is the one that matches this bottleneck."
