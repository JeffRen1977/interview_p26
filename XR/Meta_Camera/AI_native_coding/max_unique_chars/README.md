# Maximize Unique Characters — 词表子集与位运算剪枝

> 样题 3 的可跑复刻（LeetCode 1239 的工程化版本）。`project/` 是**面试开局状态**。

```bash
cd max_unique_chars
python3 -m unittest discover -s tests -v                       # 测 project/ → 3 FAIL + 10 ERROR
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 16 OK
```

---

## 1. 题面与契约

从一组词里挑一个子集，使它们**拼接后所有字符互不重复**，最大化拼接长度。

```
project/
├── wordlist.py   # sanitize()（Phase 1 bug ×2）、unique_char_count()
└── solver.py     # word_mask / popcount（已给）
                  # max_unique_length          （Phase 2 桩）
                  # max_unique_length_fast     （Phase 3 桩）
tests/test_max_unique_chars.py  # Phase1Sanitize / Phase2Backtracking / Phase3Scale
```

```python
sanitize(words: Iterable[str]) -> List[str]
unique_char_count(text: str) -> int
max_unique_length(words: Iterable[str]) -> int        # 内部自己调 sanitize
max_unique_length_fast(words: Iterable[str]) -> int   # 同样答案，但要扛住压力集
word_mask(word: str) -> int                            # 已给：26 位字母集合
```

`sanitize` 的契约（面试里要复述一遍）：trim → 小写 → 只保留纯 ASCII a–z → 丢掉**自身有重复字母**的词（它永远不可能进入答案）→ 保序、保留重复词。

---

## 2. Phase 1 — intake 的两个 bug（目标 8 分钟）

```
FAIL test_trims_lowercases_and_drops_invalid_words
FAIL test_rejects_letters_outside_ascii_a_to_z
FAIL test_unique_char_count_counts_distinct_characters
```

### Bug #1：`sanitize` 忘了小写 + `isalpha()` 不等于 ASCII

```python
word = word.strip()                 # ← 没有 .lower()
if not word or not word.isalpha():  # ← "café".isalpha() 是 True
```

**口述**（这段话值一个 Phase）：

> "Two separate defects in one line. First, it never lower-cases, so `'aA'` looks like two distinct characters and survives the duplicate check — and every mask downstream does `ord(c) - ord('a')`, so an uppercase letter would index a *negative* bit and silently corrupt the search. Second, `str.isalpha()` is True for `'café'` and for CJK; the contract says ASCII a–z, so it needs `isascii()` too."

修复：

```python
word = word.strip().lower()
if not word or not word.isascii() or not word.isalpha():
    continue
```

### Bug #2：`unique_char_count` 返回 `len(text)` 而不是 `len(set(text))`

一行改完。但要说出**为什么这个 bug 在 Phase 2 之前不可见** —— 它只在有重复字符时才错，而 `sanitize` 之后的词恰好都没有重复字符。这种"被上游掩盖的 bug"是面试官爱听的观察。

---

## 3. Phase 2 — 回溯基线（目标 12 分钟）

```python
def max_unique_length(words):
    candidates = sanitize(words)
    def search(i, used: set) -> int:
        if i == len(candidates):
            return len(used)
        best = search(i + 1, used)                 # skip
        letters = set(candidates[i])
        if not (letters & used):                   # take
            best = max(best, search(i + 1, used | letters))
        return best
    return search(0, set())
```

测试钉死两件事：

- `max_unique_length` **自己负责 sanitize**（传进去的是原始词表）
- **贪心取最长词是错的**：`["abcd", "ab", "cef"]` 的答案是 5（`ab`+`cef`），不是 4。测试 `test_taking_the_longest_word_first_is_not_optimal` 专门抓这个 —— 如果 AI 给你一个"按长度排序后贪心"的版本，这条会红。

还有一条 differential test：随机小输入上必须与 `itertools.combinations` 暴力枚举一致。**主动指出这条就是你后面验证优化正确性的工具。**

---

## 4. Phase 3 — 两组对抗样本（目标 15–18 分钟）

| 压力测试 | 形状 | 为什么 Phase 2 会死 |
|----------|------|---------------------|
| `test_many_short_words` | 12 字母表上的**全部 66 个两字母词** | 2⁶⁶ 个子集 |
| `test_a_few_very_long_words_buried_in_junk` | 3,000 个 200 字符的垃圾词 + 2 个真词 | 每层 `set` 拷贝，常数爆炸 |
| `test_duplicate_words_do_not_multiply_the_work` | 10,000 个词但只有 2 种 | 重复词让子集数指数翻倍 |

**先说瓶颈再动手**：

> "The subset space is 2^66, but the *state* space isn't. Two different subsets that cover the same letters are the same state, and there are at most 2^26 letter sets — for this input only 2^12. So I'll do DP over reachable masks instead of over subsets, with `mask & other` as the O(1) compatibility test instead of set intersection."

```python
masks = {word_mask(w): None for w in sanitize(words)}   # 顺手去重：重复词只算一次
reachable = {0}
best = 0
for mask in masks:
    grown = []
    for state in reachable:
        if state & mask:            # 冲突，O(1)
            continue
        combined = state | mask
        if combined not in reachable:
            grown.append(combined)
            best = max(best, popcount(combined))
    reachable.update(grown)
return best
```

三个要点，每个都要说出来：

1. **状态去重**：把"子集"换成"字母集合"，66 个词 × 4096 个状态 ≈ 27 万次操作；
2. **重复词折叠**：`masks` 用 dict/set 去重，10,000 个词塌缩成 2 个 mask；
3. **位运算代替集合**：`state & mask` 是一条整数指令，`set & set` 是一次哈希遍历。

> **什么时候该上 Trie？** 这道题的瓶颈在"子集组合"，不在"字符串前缀"，所以 Trie 帮不上忙。如果题目变成"从长文本里切出互不重叠的词"，那才是 Trie 的场子。**主动说出"我考虑过 Trie 但它不是这题的瓶颈"，比硬套一个 Trie 强得多。**

---

## 5. 收尾：你自己该补的测试

- differential：`max_unique_length_fast` vs `max_unique_length` 在随机小输入上必须一致（测试里已有）
- 全部词都非法 → 0；空输入 → 0
- 单个 26 字母词 → 26（上界）
- 全部词两两冲突 → 最长单词的长度
- 极端：一个词长度 27（必然含重复）→ 被 sanitize 丢掉

---

## 6. Prompt 序列

见 [`prompts.md`](./prompts.md)。
