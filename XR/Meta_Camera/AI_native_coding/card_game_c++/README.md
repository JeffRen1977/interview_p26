# Card Game (C++) — 和为 15 的策略判定系统

> Python 版 [`../card_game/`](../card_game/) 的 C++17 复刻。契约、Phase 划分、埋的 bug 一致；**Phase 3 多了一个 Python 里不存在的坑：整数溢出**。`project/` 是**面试开局状态**。

```bash
cd card_game_c++
make test                     # 测 project/ → 2 FAIL + 18 ERROR / 26
make test IMPL=solution       # 测 solution/ → 26 OK
make clean
```

---

## 1. 题面与契约

桌面上一堆牌（点数 1–9 + 花色），每次挑 **3 张和为 15** 的牌得 1 分，直到无牌可挑。

```
project/
├── card.h/.cpp        # Card：值类型、可排序（已给）
├── table.h/.cpp       # Table：多重集，contains_all / remove（已给）
├── rng.h              # Rng：确定性随机（已给）
├── game_engine.h/.cpp # is_valid_move  ← Phase 1 bug
│                      # play_out       （Phase 2 桩）
├── strategy.h/.cpp    # GreedyStrategy::choose_move   （Phase 2 桩）
│                      # count_triplets_sum_15         （Phase 3 桩）
│                      # FastStrategy::choose_move     （Phase 3 桩）
│                      # all_triplet_ranks             （已给，13 组）
└── simulation.h/.cpp  # deal（已给）/ monte_carlo（Phase 3 桩）
tests/test_card_game.cpp
    # Phase1RuleValidation / Phase2GreedyStrategy / Phase3ScaleAndEdges
```

```cpp
bool  GameEngine::is_valid_move(const std::vector<Card>&) const;
int   GameEngine::apply_move(const std::vector<Card>&);      // 非法则 throw InvalidMove
int   GameEngine::play_out(Strategy&, int max_moves = -1);   // -1 = 不限
std::optional<Triplet> Strategy::choose_move(const Table&);
long long count_triplets_sum_15(const std::vector<Card>&);
MonteCarloStats monte_carlo(Strategy&, int games, int table_size, unsigned seed);
```

**关键契约**：桌面是**多重集** —— 同一个 `(rank, suit)` 可以出现多次（牌靴由多副牌组成）。所以"这张牌在不在桌上"必须按**计数**判断，不能按"存在性"判断。`Table::contains_all` 已经替你写对了。

---

## 2. Phase 1 — 身份 vs 值（目标 8–10 分钟）

```
FAIL test_card_not_on_the_table_is_rejected
FAIL test_same_card_cannot_be_used_twice
```

```cpp
std::vector<int> table_ranks;
for (const Card& card : table_.cards()) table_ranks.push_back(card.rank);
for (const Card& card : cards) {
    if (std::find(table_ranks.begin(), table_ranks.end(), card.rank) == table_ranks.end()) {
        return false;
    }
}
```

这段代码把"牌在不在桌上"退化成了"**这个点数**在不在桌上"，于是两件事同时坏掉：

1. **身份丢了**：桌上是 `7S`，你打 `7H` 也能过 —— 点数一样，牌不一样；
2. **计数丢了**：桌上只有一张 `5C`，你打 `5C + 5C + 5C` 照样过 —— 因为它只问"5 在不在"，问了三次，三次都在。

修复是一行，把整段换成 `return table_.contains_all(cards);`。

**口述**：

> "It projects each card down to its rank and then does a membership test on a vector of ranks. That loses two things at once: identity, so a 7H passes because a 7S is on the table; and multiplicity, so the same 5C can be consumed three times. Availability on a multiset is a *count* question — `Table::contains_all` already answers it correctly, so `is_valid_move` should just delegate."

> **一个 C++ 特有的观察**：`Card` 是值类型且定义了 `operator==`，所以 `std::find(cards.begin(), cards.end(), card)` 本来就能做正确的身份比较 —— 这个 bug 完全是**主动**把 `card` 降维成 `card.rank` 造成的。这类"顺手取个字段来比"的写法在 C++ 里特别常见，因为容器里放的往往是索引或 key 而不是对象本身。review 时看到 `find(..., x.field)` 就该停一下。

`test_duplicate_is_legal_when_the_table_really_holds_two_copies` 是配对的反向测试：桌上**真有**两张 `5C` 时，`5C + 5C + 5H` 必须合法。修 bug 时如果图省事写成 `std::set<Card>`，这条会红。

---

## 3. Phase 2 — 贪心 + 结算循环（目标 12–15 分钟）

`GreedyStrategy::choose_move` 是三重循环枚举**位置**（不是值）：

```cpp
for (std::size_t i = 0; i + 2 < n; ++i)
  for (std::size_t j = i + 1; j + 1 < n; ++j)
    for (std::size_t k = j + 1; k < n; ++k)
      if (cards[i].rank + cards[j].rank + cards[k].rank == kTargetSum)
          return Triplet{cards[i], cards[j], cards[k]};
return std::nullopt;
```

> **注意 `i + 2 < n` 而不是 `i < n - 2`。** `n` 是 `std::size_t`，空表时 `n - 2` 会**回绕成一个巨大的数**，循环直接越界。这是 C++ 里最经典的无符号陷阱之一，`test_choose_move_on_empty_table` 专门抓它。

`play_out` 的循环：

```cpp
while (max_moves < 0 || score_ < max_moves) {
    const auto move = strategy.choose_move(table_);
    if (!move.has_value()) break;
    apply_move({move->begin(), move->end()});   // 重新校验：策略有 bug 也不许得分
}
return score_;
```

**要说出口的设计决策**：`play_out` 拿到策略的返回值后**再走一遍 `apply_move`**，而不是直接删牌加分。这样一个有 bug 的策略只会抛 `InvalidMove`，不会污染分数 —— **引擎不信任策略**。

最值钱的一条测试是 `test_play_out_conserves_cards_and_reaches_a_terminal_state`：20 张随机桌面，检查三条**不变量**而不是具体走法 ——

- 剩余牌数 == 原牌数 − 3 × 得分（没有牌被凭空造出来或丢掉）；
- 被消耗的牌数恰好等于 3 × 得分；
- 停下来时桌面上**确实**再没有和为 15 的三张牌。

主动指出"我用不变量校验而不是硬编码期望走法"，这是 Senior 信号。

---

## 4. Phase 3 — 频次表 + 组合数（目标 15–18 分钟）

| 压力测试 | 形状 | 为什么 Phase 2 会死 |
|---|---|---|
| `test_count_is_fast_on_a_huge_table` | 20,000 张随机牌 | `C(20000,3) ≈ 1.3×10¹²` 次枚举 |
| `test_fast_strategy_survives_the_adversarial_table` | 2,000 张 9 藏着 9 张 5 | 要枚举 `C(2009,3)` 才敢说"这些 9 没救了" |
| `test_count_stays_in_64_bit` | 6,000 张 5 | **结果 3.6×10¹⁰，装不进 `int`** |

**先说复杂度再动手**：

> "Ranks only span 1..9, so the search space isn't the cards, it's the ranks. There are exactly 13 non-decreasing rank triples summing to 15 — I'll count with a frequency table and binomials instead of enumerating cards."

```cpp
long long count_triplets_sum_15(const std::vector<Card>& cards) {
    const auto freq = rank_freq(cards);              // O(n)
    long long total = 0;
    for (const auto& [r1, r2, r3] : all_triplet_ranks()) {   // 13 组
        const long long f1 = freq[r1], f2 = freq[r2], f3 = freq[r3];
        if (r1 == r2 && r2 == r3)  total += choose3(f1);     // (5,5,5)
        else if (r1 == r2)         total += choose2(f1) * f3; // (1,7,7)
        else if (r2 == r3)         total += f1 * choose2(f2); // (3,6,6)
        else                       total += f1 * f2 * f3;
    }
    return total;
}
```

`O(n)` 建频次表 + `O(13)` 枚举 —— **与合法组合的数量无关**。对抗样例里那 `1.3×10¹²` 个组合，一个都不用碰。

**三种退化情况必须分开处理**：直接写 `f1*f2*f3` 在 `(5,5,5)` 上会算成 `f5³`（把同一张牌数了三次）。`test_count_on_edge_inputs` 里 `count(6 张 5) == 20 == C(6,3)` 就是抓这个的。

### C++ 版的额外考点：整数宽度

这是 Python 版**完全不存在**的一关。Python 的 `int` 是任意精度，`C(6000,3)` 随便算；C++ 里：

```cpp
C(6000, 3)              = 35,982,002,000      // > 2^31，装不进 int
6000 * 5999 * 5998      = 215,892,012,000     // 中间量更大
```

而且**有符号溢出是未定义行为**，不是"回绕成负数"这种你能预测的行为 —— 编译器有权假设它不发生并据此优化掉你的检查。

所以：

- 返回类型是 `long long`；
- `choose2` / `choose3` 的**中间量**也必须是 `long long`（`n * (n-1) * (n-2) / 6`，先乘后除，且三个数里必有一个能被 2 整除、一个能被 3 整除，所以整除是精确的）；
- 频次表用 `std::array<long long, 10>` 而不是 `int`。

`test_count_stays_in_64_bit` 钉死这条。**主动说出"这里必须 64 位，而且溢出是 UB 不是回绕"，是这一轮的 C++ 加分点。**

### FastStrategy：把 13 组模式落到桶上

```cpp
std::map<int, std::vector<Card>> buckets;             // rank -> cards
for (const Card& card : table.cards()) buckets[card.rank].push_back(card);
for (const auto& [r1, r2, r3] : all_triplet_ranks()) {
    std::map<int, int> need;  ++need[r1]; ++need[r2]; ++need[r3];
    // 每个 rank 的库存够不够
}
```

关键收益：**"无解"的判定也只要 13 次查表**，而不是枚举完所有组合才敢返回 `nullopt`。这正是对抗样例攻击的点。

注意 `need` 要用计数而不是三个独立的查询 —— `(5,5,5)` 需要**三张** 5，桶里只有两张时不能返回。

### Monte Carlo：确定性

```cpp
MonteCarloStats monte_carlo(Strategy& strategy, int games, int table_size, unsigned seed) {
    Rng rng(seed);                       // 一个 rng 从头用到尾，只 seed 一次
    ...
}
```

**别**在函数里新建多个 `Rng`，**别**用全局 `std::rand()`，**别**碰时钟。`test_monte_carlo_is_deterministic_for_a_seed` 跑两遍比结构体相等。

> `rng.h` 里刻意没用 `std::uniform_int_distribution` —— **它的输出在不同标准库实现之间不可移植**（`std::mt19937` 引擎本身是可移植的，分布不是）。要跨平台复现的随机，就得自己取模。这一句值得主动说。

---

## 5. 收尾：你自己该补的测试

- 空桌 / 只有 1–2 张牌 → `choose_move` 返回 `nullopt`，`count` 返回 0
- 全是同一张牌（`9` × 100）→ 27 ≠ 15，计数为 0
- `max_moves = 0` → 得分 0，桌面不动
- 策略返回一个不在桌上的三元组 → `play_out` 必须抛 `InvalidMove` 而不是静默计分
- `Card(10, 'S')` / `Card(5, 'X')` → `std::invalid_argument`
- 用 sanitizer 跑一遍：`make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"` —— 如果 §4 那个计数被写成了 `int`，UBSan 会当场把溢出报出来

---

## 6. Prompt 序列

见 [`prompts.md`](./prompts.md)。
