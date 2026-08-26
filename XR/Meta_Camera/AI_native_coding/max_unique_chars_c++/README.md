# Maximize Unique Characters (C++) — 词表子集与位运算状态压缩

> Python 版 [`../max_unique_chars/`](../max_unique_chars/) 的 C++17 复刻。契约、Phase 划分一致；**Phase 1 埋的 bug 换成了 C++ 里更真实的三个**。`project/` 是**面试开局状态**。

```bash
cd max_unique_chars_c++
make test                     # 测 project/ → 4 FAIL + 10 ERROR / 18
make test IMPL=solution       # 测 solution/ → 18 OK
make clean
```

---

## 1. 题面与契约

从一组词里挑一个子集，使它们**拼接后所有字符互不重复**，最大化拼接长度（LeetCode 1239 的工程化版本）。

```
project/
├── wordlist.h/.cpp   # sanitize / unique_char_count   ← Phase 1 bug ×3
└── solver.h/.cpp     # word_mask / popcount（已给且正确）
                      # max_unique_length        （Phase 2 桩）
                      # max_unique_length_fast   （Phase 3 桩）
tests/test_max_unique_chars.cpp
    # Phase1Sanitize / Phase2Backtracking / Phase3Scale
```

```cpp
std::vector<std::string> sanitize(const std::vector<std::string>& words);
int unique_char_count(const std::string& text);
int word_mask(const std::string& word);          // 已给
int popcount(int mask);                          // 已给
int max_unique_length(const std::vector<std::string>& words);
int max_unique_length_fast(const std::vector<std::string>& words);
```

`sanitize` 的契约（面试里要复述一遍）：**两端**去空白 → 转小写 → 只保留纯 ASCII `a-z` → 丢掉**自身有重复字母**的词（它永远不可能进入答案）→ 保序、保留重复词。

两个求解函数拿到的是**原始词表**，自己负责 sanitize。

---

## 2. 一个先讲清楚的语言差异

Python 版里 `word_mask` 是这样的：

```python
mask |= 1 << (ord(char) - ord("a"))
```

如果一个大写字母漏过了 `sanitize`，`1 << -65` 会**直接抛 `ValueError`** —— Python 帮你把上游的 bug 顶了出来。

C++ 里同样的写法是 `1 << (ch - 'a')`，**负数移位是未定义行为**：它不会抛异常，只会悄悄给你一个垃圾 mask，然后你会在 Phase 3 调试一个根本不在那里的 bug。

所以这里的 `word_mask` 是**给定且防御性**的 —— 它忽略 `a`..`z` 之外的字节，把"拒绝非法字符"的责任完全交给 `sanitize`：

```cpp
int word_mask(const std::string& word) {
    int mask = 0;
    for (char ch : word) {
        if (ch >= 'a' && ch <= 'z') mask |= 1 << (ch - 'a');
    }
    return mask;
}
```

**这段话本身就该在面试里说出来。** 它演示了一件事：同一个上游 bug，在 Python 里是一次崩溃，在 C++ 里是一次静默的数据损坏。语言的"失败模式"不同，防御的位置就不同。

---

## 3. Phase 1 — 三个 intake bug（目标 8–10 分钟）

```
FAIL test_trims_whitespace_from_both_ends
FAIL test_lowercases_before_the_duplicate_check
FAIL test_trims_lowercases_and_drops_invalid_words
FAIL test_unique_char_count_counts_distinct_characters
```

（四条红线，三个 bug —— 第三条是前两个的合并场景。）

### Bug #1：`trim` 只砍了前面

```cpp
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first);          // ← 尾部空白还在
}
```

`"\tXYZ\n"` 变成 `"XYZ\n"`，然后因为 `\n` 不是 `a-z` **被整个丢掉**了——它本该变成 `"xyz"` 留下来。

注意这个 bug 的表现形式：不是"结果多了个空格"，而是**一个合法的词凭空消失**。上游的清洗错误在下游表现为"数据变少"，这类症状最难往回追。

**口述**：

> "`find_first_not_of` gives me the front; there's no matching `find_last_not_of`, so trailing whitespace survives. That's not a cosmetic bug — the trailing `\n` then fails the a-z test and the whole word is silently dropped."

### Bug #2：没有转小写

`"aA"` 应该先变成 `"aa"`，然后因为重复字母被丢掉。不转小写的话，`'a'` 和 `'A'` 是两个不同的字节，去重检查放它过去了。而且留下来的词还带着大写，会一路流到 `word_mask` 面前（见 §2）。

修复要放在**去重检查之前**：

```cpp
for (char& ch : word) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}
```

> **注意那个 `static_cast<unsigned char>`。** `std::tolower` 的参数必须能表示为 `unsigned char` 或 `EOF`；直接传一个可能为负的 `char` 是**未定义行为**。这是 C++ 里最常被忽略的一行样板，AI 生成的代码经常漏。写上它，并且说出来。

### Bug #3：`unique_char_count` 返回的是长度

```cpp
int unique_char_count(const std::string& text) {
    return static_cast<int>(text.size());   // ← 契约要的是"不同字符数"
}
```

一行改成 `std::unordered_set<char>` 的 size。

> **Python 版的第二个 bug 在 C++ 里不存在。** 那边 `str.isalpha()` 对 `"café"`、`"日本"` 都返回 True，所以必须额外加 `isascii()`；C++ 这边按字节比较 `ch >= 'a' && ch <= 'z'`，多字节序列天然被拒。`test_rejects_bytes_outside_ascii_a_to_z` 留着当契约锚点，它开局就是绿的 —— **面试里指出"这条在 C++ 里是免费的"也是一个观察点**。

---

## 4. Phase 2 — 回溯（目标 12–15 分钟）

```cpp
void backtrack(const std::vector<int>& masks, std::size_t idx, int state, int length, int& best) {
    if (length > best) best = length;
    for (std::size_t i = idx; i < masks.size(); ++i) {
        if ((state & masks[i]) == 0) {
            backtrack(masks, i + 1, state | masks[i], length + popcount(masks[i]), best);
        }
    }
}
```

三件事测试会抓：

- **自己负责 sanitize**：`max_unique_length({" UN ", "iq", "ue", "aa", "x1"})` 必须是 4；
- **贪心取最长词是错的**：`{"abcd", "ab", "cef"}` 的答案是 5（`ab`+`cef`），不是 4。`test_taking_the_longest_word_first_is_not_optimal` 专抓这个 —— 如果 AI 给你一个"按长度排序后贪心"的版本，这条会红；
- **对拍**：`test_matches_brute_force_on_random_small_inputs` 拿一个 `2^n` 的暴力枚举当裁判，30 组随机输入。

> **C++ 细节**：`best` 用引用传递而不是返回值累加。也可以用 `static thread_local`，但那会让函数不可重入 —— 说出这个取舍。

---

## 5. Phase 3 — 状态去重（目标 15 分钟）

| 压力测试 | 形状 | 为什么 Phase 2 会死 |
|----------|------|---------------------|
| `test_many_short_words` | 12 字母表上**全部 66 个两字母词** | `2^66` 个子集 |
| `test_a_few_very_long_words_buried_in_junk` | 3,000 个 200 字符的垃圾词 + 2 个真词 | 每层拷贝字符串，常数爆炸 |
| `test_duplicate_words_do_not_multiply_the_work` | 10,000 个词但只有 2 种 | 重复词让子集数指数翻倍 |

**先说瓶颈再动手**：

> "The subset space is 2^66, but the *state* space isn't. Two different subsets that cover the same letters are the same state, and there are at most 2^26 letter sets — for this input only 2^12. So I'll do DP over reachable masks instead of over subsets, with `state & mask` as the O(1) compatibility test instead of a set intersection."

```cpp
std::unordered_set<int> unique_masks;               // 重复词在这里塌缩成一个
for (const std::string& word : sanitize(words)) unique_masks.insert(word_mask(word));

std::unordered_set<int> reachable{0};
std::vector<int> fresh;
int best = 0;
for (int mask : unique_masks) {
    fresh.clear();                                   // 复用缓冲区，别每轮新建 vector
    for (int state : reachable) {
        if ((state & mask) == 0) {
            const int combined = state | mask;
            fresh.push_back(combined);
            best = std::max(best, popcount(combined));
            if (best == 26) return 26;               // 上界，提前退出
        }
    }
    reachable.insert(fresh.begin(), fresh.end());
}
```

三个收益，每个都要说出来：

1. **状态去重**：把"子集"换成"字母集合"，66 个词 × 最多 4096 个状态；
2. **重复词折叠**：`unordered_set<int>` 让 10,000 个词塌缩成 2 个 mask；
3. **上界剪枝**：`best == 26` 就可以直接返回，26 是字母表大小。

**一个 C++ 专属的坑**：不能一边遍历 `reachable` 一边往里 `insert` —— 迭代器会失效（`unordered_set` 的 rehash 会让所有迭代器失效）。必须先收集到 `fresh` 里，遍历结束后统一并入。**这是 AI 生成代码在这道题上最常见的一处 UB**，跑 `-fsanitize=address` 能抓到。

> **什么时候该上 Trie？** 这道题的瓶颈在"子集组合"，不在"字符串前缀"，所以 Trie 帮不上忙。如果题目变成"从长文本里切出互不重叠的词"，那才是 Trie 的场子。**主动说出"我考虑过 Trie 但它不是这题的瓶颈"，比硬套一个 Trie 强得多。**

---

## 6. 收尾：你自己该补的测试

- 全部词都非法 → 0；空输入 → 0
- 单个 26 字母词 → 26（上界）
- 全部词两两冲突 → 最长单词的长度
- 一个长度 27 的词（必然含重复）→ 被 sanitize 丢掉
- `std::tolower` 的 `unsigned char` 转换（主动提，测试没覆盖）
- 用 sanitizer 跑一遍：`make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"` —— 专抓 §5 那个迭代器失效

---

## 7. Prompt 序列

见 [`prompts.md`](./prompts.md)。
