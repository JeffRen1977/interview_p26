# Card Game — 逐阶段 Prompt

按顺序用。每个 Phase 结束先跑测试再进下一个。

---

## Prompt 0 — 抽 spec（不写代码）

```
Read models/card.py, table.py, game_engine.py, strategy.py and tests/test_card_game.py.
Do NOT write implementation code.

List:
1. The public API I must implement (exact signatures and return types)
2. What each test class asserts, as invariants
3. The edge cases the tests imply (empty table, duplicate cards, all-same-rank, no legal move)
4. Anything the tests do NOT require, so we don't build it
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
These fail:
  FAIL test_card_not_on_the_table_is_rejected
  FAIL test_same_card_cannot_be_used_twice
  ERROR test_apply_move_rejects_an_illegal_move

Explain the root cause in GameEngine.is_valid_move in terms of the Table
contract in table.py. Which single block is wrong and why?
Do NOT propose code yet.
```

> 先自己说出答案，再看它同不同意。它说对了你确认，说错了你纠正 —— 面试官看的就是这个。

---

## Prompt 2 — Phase 1 最小修复

```
Task: fix GameEngine.is_valid_move only.
Constraints:
- Signature stays is_valid_move(self, cards: Sequence[Card]) -> bool
- Use the multiset primitive Table already exposes; do not reimplement counting
- A move IS legal when the table genuinely holds two copies of the same card
- Do NOT modify Card, Table, or any other method
Return only the replacement method body.
```

跑：`python3 -m unittest discover -s tests -k Phase1 -v`

---

## Prompt 3 — Phase 2 贪心 + 结算

```
Task: implement GreedyStrategy.choose_move in strategy.py and
GameEngine.play_out in game_engine.py.
Constraints:
- choose_move(table) -> Optional[Tuple[Card, Card, Card]]: first combination of
  3 distinct table positions whose ranks sum to 15, else None
- play_out(strategy, max_moves=None) -> int: loop choose_move, re-validate with
  is_valid_move, apply_move, stop on None or when max_moves moves were made
- Return the accumulated score
- Do NOT change Strategy's abstract signature, Card, or Table
- Do NOT import time or random
Return only the two function bodies.
```

跑：`python3 -m unittest discover -s tests -k Phase2 -v`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code, do not rename
anything, do not touch Phase 3 stubs.
<paste unittest output>
```

---

## Prompt 5 — Phase 3 计数（先说瓶颈）

```
count_triplets_sum_15 must handle 20,000 cards under 1 second, so enumerating
C(n,3) is out. Ranks are integers in 1..9.

Task: implement count_triplets_sum_15(cards) -> int using a rank frequency
table and binomial coefficients over the <=84 non-decreasing rank triples
summing to 15 (helper all_triplet_ranks() already exists).
Constraints:
- Handle the degenerate cases correctly: r1==r2==r3 -> C(f,3);
  r1==r2<r3 -> C(f1,2)*f3; r1<r2==r3 -> f1*C(f2,2)
- Signature unchanged; no new dependencies beyond collections/math
Return only the function body.
```

---

## Prompt 6 — Phase 3 快速选牌

```
The adversarial table is 2,000 nines plus nine fives: the brute-force strategy
has to enumerate C(2009,3) combinations just to prove the nines are dead.

Task: implement FastStrategy.choose_move with the same contract as
GreedyStrategy, but O(n) bucketing + O(84) scan per decision, so returning None
is as cheap as returning a move.
Constraints:
- Bucket cards by rank; for each rank triple check multiplicity with a Counter
- Must respect multiplicity: (5,5,5) needs three cards in bucket 5
- Same signature; do not change GreedyStrategy
Return only the class body.
```

---

## Prompt 7 — Phase 3 Monte Carlo

```
Task: implement monte_carlo(strategy, games, table_size, seed) in simulation.py.
Constraints:
- One random.Random(seed) created inside the function and used for every deal,
  so the same seed always replays identical tables
- Never touch the global random module or time
- Return {"games", "total_score", "mean_score", "max_score", "zero_score_games"}
- games=0 or a table too small to score must not raise
Return only the function body.
```

跑全量：`AINC_IMPL=project python3 -m unittest discover -s tests -v`

---

## Prompt 8 — 收尾 review（跑绿之后）

```
Review game_engine.py and strategy.py against the tests only.
Call out: dead code you added, any remaining O(n^3) path, and behaviour that no
test covers. Do NOT change code unless I ask.
```

然后你自己说：

> "One thing the suite doesn't pin down: greedy and the fast strategy can reach
> different scores on the same table, because the set they clear first changes
> what's left. If maximising score mattered we'd need search, not greed — worth
> flagging as a product decision rather than a bug."
