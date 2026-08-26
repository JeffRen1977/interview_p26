# Card Game (C++) — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read card.h, table.h, game_engine.h, strategy.h, simulation.h and
tests/test_card_game.cpp. Do NOT write implementation code.

List:
1. Why Table is described as a MULTISET and what that implies for "is this
   card available"
2. The exact contract of is_valid_move, apply_move and play_out, including
   what max_moves = -1 means
3. What count_triplets_sum_15 counts (subsets by position? by value?) and why
   its return type is long long
4. What the three Phase-3 stress shapes are and what each one is punishing
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Two Phase-1 failures:
  test_card_not_on_the_table_is_rejected   (7H passes while only 7S is on the table)
  test_same_card_cannot_be_used_twice      (5C+5C+5C passes with one 5C on the table)

Explain the single root cause in is_valid_move in one sentence, naming the
exact line. Say which TWO distinct properties that line destroys.
Note that Table already exposes a method that answers the question correctly.
Do NOT propose a fix yet.
```

期望它说出：把 `card` 投影成 `card.rank` 再做存在性查找，同时丢掉了**身份**（7H vs 7S）和**重数**（一张 5C 被消费三次）；`Table::contains_all` 已经按多重集实现好了。

---

## Prompt 2 — Phase 1 修复

```
Task: fix GameEngine::is_valid_move in game_engine.cpp.
Constraints:
- Keep the size check and the sum check as they are
- Availability must be a MULTISET containment test: delegate to
  Table::contains_all, which already handles counts
- Do NOT switch to std::set<Card> or std::unique -- the table legitimately holds
  duplicate (rank, suit) cards and a move may legitimately use two of them
- Signature and const-ness unchanged; do not touch table.cpp or card.cpp
Return only the changed function.
```

跑：`make test`

---

## Prompt 3 — Phase 2 贪心 + 结算循环

```
Task: implement GreedyStrategy::choose_move and GameEngine::play_out.

choose_move constraints:
- Enumerate three distinct POSITIONS i<j<k on the table (not three values) and
  return the first triple whose ranks sum to kTargetSum, else std::nullopt
- Loop bounds must be written as `i + 2 < n`, NOT `i < n - 2`: n is size_t and
  n - 2 wraps to a huge value on an empty table
- Do not sort or otherwise reorder the table

play_out constraints:
- Loop choose_move; stop on nullopt or when max_moves is reached
- max_moves < 0 means unlimited
- Route every move through apply_move so a buggy strategy throws InvalidMove
  instead of silently scoring -- the engine does not trust the strategy
- Return the total score
- Do NOT implement FastStrategy, count_triplets_sum_15 or monte_carlo
Return only the two functions.
```

跑：`make test`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste the failing test name and its message>
```

---

## Prompt 5 — Phase 3 计数（把整数宽度写进 prompt）

```
Ranks span only 1..9, so the search space is the ranks, not the cards: exactly
13 non-decreasing rank triples sum to 15, and all_triplet_ranks() returns them.

Task: implement count_triplets_sum_15(cards) -> long long.
Constraints:
- Build a rank frequency table in O(n), then walk the 13 triples
- Handle the three degenerate shapes separately:
    r1==r2==r3 -> C(f,3)      (5,5,5)
    r1==r2     -> C(f1,2)*f3  (1,7,7)
    r2==r3     -> f1*C(f2,2)  (3,6,6)
    otherwise  -> f1*f2*f3
  Writing f1*f2*f3 for (5,5,5) counts the same card three times
- EVERY intermediate stays long long: C(6000,3) is 3.6e10 and the numerator
  n*(n-1)*(n-2) is 2.2e11. int overflows, and signed overflow is undefined
  behaviour, not a wrap
- The frequency table itself must be long long, not int
- Compute n*(n-1)*(n-2)/6 as multiply-then-divide; the product of three
  consecutive integers is always divisible by 6, so it is exact
Return only the function plus anonymous-namespace helpers.
```

跑：`make test`

---

## Prompt 6 — Phase 3 FastStrategy

```
Task: implement FastStrategy::choose_move with the same observable behaviour as
GreedyStrategy but O(9^3) per decision.
Constraints:
- Bucket the table by rank into std::map<int, std::vector<Card>>
- For each of the 13 rank triples, check STOCK not presence: (5,5,5) needs three
  cards of rank 5, so count how many of each rank the triple requires
- Materialise the triplet from the buckets and return it; nullopt if none fits
- Returning nullopt must mean "no triplet exists", which the tests verify
  against a brute-force check
- Do not change GreedyStrategy -- I want to differential-test them
Return only the function.
```

## Prompt 7 — Phase 3 Monte Carlo

```
Task: implement monte_carlo(strategy, games, table_size, seed) -> MonteCarloStats.
Constraints:
- Construct exactly ONE Rng(seed) and thread it through every deal, so the same
  seed reproduces the same stats bit for bit
- Do NOT use std::rand, a global generator, std::random_device, or any clock
- Fill in games, total_score (long long), mean_score, max_score,
  zero_score_games
- games == 0 must not divide by zero
Return only the function.
```

跑：`make test`，然后

```bash
make clean
make test IMPL=solution CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"
```

---

## Prompt 8 — 复杂度与溢出确认（只问，不写）

```
State the complexity of count_triplets_sum_15 in terms of the number of cards n
and tell me what it is INDEPENDENT of. Then tell me the largest value it can
return for n = 20000 and whether that fits in int32.
Do NOT change code.
```

期望答案：`O(n)` 建表 + `O(13)` 枚举，**与合法组合的数量无关**；20000 张牌上界约 `C(20000,3) ≈ 1.3×10¹²`，远超 int32。

---

## Prompt 9 — 收尾 review

```
Review game_engine.cpp, strategy.cpp and simulation.cpp against the tests only.
Call out: any arithmetic that could overflow int, any unsigned loop bound of
the form `i < n - k`, any use of a global or clock-seeded random source, dead
code, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "Two things I'd flag. First, `rng.h` deliberately avoids
> `std::uniform_int_distribution` — the mt19937 *engine* is portable across
> standard libraries but the *distributions* are not, so a seeded run wouldn't
> reproduce across platforms. Second, the whole Phase-3 speedup rests on the
> rank domain being 1..9 and known at compile time. If ranks became unbounded,
> the 13 patterns become O(V²) patterns and I'd switch to sort-plus-two-pointer
> per anchor — same idea, different constant."
