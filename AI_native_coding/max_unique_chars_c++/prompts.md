# Maximize Unique Characters (C++) — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read wordlist.h/.cpp, solver.h/.cpp and tests/test_max_unique_chars.cpp.
Do NOT write implementation code.

List:
1. The exact sanitize contract: what is trimmed, what is lower-cased, which
   characters are legal, which words are dropped, what order is preserved
2. Whether max_unique_length receives raw or sanitized input
3. Why word_mask ignores bytes outside 'a'..'z' instead of shifting by a
   negative amount
4. What the three Phase-3 stress shapes are and what each one is punishing
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Four Phase-1 failures, three bugs:
  test_trims_whitespace_from_both_ends
  test_lowercases_before_the_duplicate_check
  test_trims_lowercases_and_drops_invalid_words
  test_unique_char_count_counts_distinct_characters

Explain each root cause in one sentence, naming the exact line in wordlist.cpp.
For the trim bug, say what the OBSERVABLE symptom is downstream -- it is not
"an extra space", it is something worse.
Do NOT propose code yet.
```

期望它说出：`find_first_not_of` 没有配对的 `find_last_not_of`，尾部空白让整个词在 a-z 检查里**被丢掉**；没转小写导致 `"aA"` 通过去重检查；`unique_char_count` 返回的是 `size()`。

---

## Prompt 2 — Phase 1 修复

```
Task: fix sanitize and unique_char_count in wordlist.cpp.
Constraints:
- trim must strip whitespace from BOTH ends (find_first_not_of +
  find_last_not_of)
- lower-case BEFORE the duplicate-letter check, otherwise "aA" survives
- std::tolower takes an int that must be representable as unsigned char:
  cast with static_cast<unsigned char> before calling it, passing a plain
  (possibly negative) char is undefined behaviour
- Legality stays a byte-wise 'a'..'z' test; do not add a locale
- unique_char_count returns the number of DISTINCT characters
- Preserve order; keep duplicate candidates
- Do not touch solver.cpp
Return only the changed functions.
```

跑：`make test`

---

## Prompt 3 — Phase 2 回溯

```
Task: implement max_unique_length(words) -> int in solver.cpp.
Constraints:
- The input is RAW. Call sanitize() yourself
- Turn each candidate into a 26-bit mask with the given word_mask()
- Backtrack over take/skip; two words are compatible iff (state & mask) == 0
- Track the best popcount seen; return 0 when nothing is usable
- Pass `best` by reference; do NOT use a static or thread_local accumulator,
  that would make the function non-reentrant
- Do NOT implement max_unique_length_fast
Return only the function plus anonymous-namespace helpers.
```

跑：`make test`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste the failing test name and its message>
```

---

## Prompt 5 — Phase 3（先说瓶颈，再让它写）

```
test_many_short_words feeds all 66 two-letter words over a 12-letter alphabet.
The subset space is 2^66, but the STATE space is not: two subsets covering the
same letters are the same state, and there are at most 2^12 letter sets here.

Task: implement max_unique_length_fast as DP over reachable letter masks.
Constraints:
- Collapse duplicate words first: std::unordered_set<int> of word masks, so
  10,000 copies of "abc" become one entry
- Keep a set of reachable states starting from {0}; for each mask, extend every
  compatible state
- DO NOT insert into `reachable` while iterating over it -- an unordered_set
  rehash invalidates every iterator. Collect into a scratch vector first and
  insert after the loop. Reuse that vector across iterations
- Return early at 26, the alphabet upper bound
- Must return exactly the same answer as max_unique_length; keep that function
  untouched so I can differential-test them
Return only the function body.
```

跑：`make test`，然后

```bash
make clean
make test IMPL=solution CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"
```

---

## Prompt 6 — 复杂度确认

```
State the complexity of max_unique_length_fast in terms of the number of
DISTINCT word masks m and the number of reachable states s. Explain why s is
bounded by 2^26 and by 2^(letters actually used), and tell me the input shape
where this DP is no better than the backtracking version.
Do NOT change code.
```

期望答案：`O(m · s)`；`s` 受字母表大小而非词数限制；当所有词两两不冲突且用满 26 个字母时 `s` 会逼近 `2^26`，此时 DP 不再占优 —— 但那种输入本身就要求 26 个互不相交的词。

---

## Prompt 7 — 收尾 review

```
Review wordlist.cpp and solver.cpp against the tests only.
Call out: any iterator held across a container mutation, any std::tolower /
std::isalpha call without an unsigned char cast, dead code, and any behaviour
no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "One language-level thing worth flagging: in the Python version of this
> problem, an uppercase letter leaking past sanitize makes `1 << (ord(c) -
> ord('a'))` raise ValueError, so the upstream bug surfaces immediately. In C++
> a negative shift is undefined behaviour — it would hand me a garbage mask and
> I'd spend Phase 3 debugging a bug that isn't there. That's why `word_mask`
> here is defensive and rejection lives entirely in `sanitize`. Same algorithm,
> different place to put the guard, because the two languages fail differently."
