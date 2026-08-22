# Card Game — 和为 15 的策略判定系统

> 样题 1 的可跑复刻。`project/` 是**面试开局状态**：Phase 1 的 bug 真的埋在代码里，Phase 2/3 是 `NotImplementedError` 桩。`solution/` 是参考实现。

```bash
cd card_game
python3 -m unittest discover -s tests -v                       # 测 project/ → 2 FAIL + 17 ERROR
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 25 OK
```

---

## 1. 工程结构与契约

```
project/
├── models/card.py   # Card(rank 1..9, suit) — frozen dataclass，不许改
├── table.py         # Table：卡牌 multiset，暴露 cards / counts / contains_all / remove
├── game_engine.py   # GameEngine.is_valid_move（Phase 1 bug）/ apply_move / play_out（Phase 2 桩）
├── strategy.py      # Strategy ABC / GreedyStrategy（P2 桩）/ count_triplets_sum_15 + FastStrategy（P3 桩）
└── simulation.py    # monte_carlo（P3 桩）
tests/test_card_game.py   # spec：Phase1RuleValidation / Phase2GreedyStrategy / Phase3ScaleAndEdges
```

Public API（prompt 里要写死的"不许改"清单）：

```python
GameEngine(table).is_valid_move(cards: Sequence[Card]) -> bool
GameEngine(table).apply_move(cards) -> int          # 返回累计得分，非法则 raise InvalidMove
GameEngine(table).play_out(strategy, max_moves=None) -> int
Strategy.choose_move(table) -> Optional[Tuple[Card, Card, Card]]
count_triplets_sum_15(cards: Sequence[Card]) -> int
monte_carlo(strategy, games, table_size, seed) -> Dict[str, float]
```

规则：一次合法操作 = **恰好 3 张、都在桌面上、点数和为 15**，结算得 1 分并把这 3 张移出桌面。

---

## 2. Phase 1 — 找 bug（目标 8–12 分钟）

跑测试，你会看到三条红线：

```
FAIL: test_card_not_on_the_table_is_rejected
FAIL: test_same_card_cannot_be_used_twice
ERROR: test_apply_move_rejects_an_illegal_move   (ValueError 从 Table.remove 漏上来)
```

`game_engine.py` 里的元凶只有 4 行：

```python
table_ranks = [card.rank for card in self.table.cards]
for card in cards:
    if card.rank not in table_ranks:
        return False
```

**口述根因**（这句话就是 Phase 1 的分）：

> "It validates *ranks*, not *cards*. Two things break: a card that isn't on the table passes as long as some card shares its rank — 7H rides in behind 7S — and the same card can be counted three times, because membership says nothing about how many copies exist. The table is a multiset, so the check has to be multiset containment."

修复（Table 已经把正确原语给你了，别自己再写一遍）：

```python
return self.table.contains_all(cards)
```

**注意**：`contains_all` 必须保留"桌面真有两张 5C 时 5C+5C+5H 合法"的语义 —— 测试里有这一条，别一刀切成"三张必须互不相同"。

---

## 3. Phase 2 — 贪心结算（目标 15 分钟）

两件事：

1. `GreedyStrategy.choose_move`：按桌面顺序返回第一个和为 15 的三张组合，没有就 `None`；
2. `GameEngine.play_out`：循环取 move → 校验 → `apply_move`，直到 `None` 或 `max_moves`。

```python
def choose_move(self, table):
    for triplet in combinations(table.cards, SET_SIZE):
        if sum(c.rank for c in triplet) == TARGET_SUM:
            return triplet
    return None
```

测试断言的是**不变量**而不是具体选了哪三张（贪心顺序相关）：

- 得分 = 出手次数，且 `len(remaining) == len(original) - 3 * score`
- 消耗的牌 multiset 加回去必须等于原始 multiset（没有牌被复制或凭空消失）
- 终局桌面上**不能还剩**任何和为 15 的组合

`play_out` 里对策略返回值再做一次 `is_valid_move` 校验 —— 这是防御性写法，面试官会注意到。

---

## 4. Phase 3 — 规模与边界（目标 15–18 分钟）

测试加了三种压力：

| 测试 | 压力 | 朴素做法为什么死 |
|------|------|------------------|
| `test_count_is_fast_on_a_huge_table` | 20,000 张牌数组合总数 | `combinations(cards, 3)` = 1.3×10¹² |
| `test_fast_strategy_survives_the_adversarial_table` | 2,000 张 9 + 9 张 5 | 证明"2000 张 9 里没有解"要枚举 C(2000,3) ≈ 1.3×10⁹ |
| `test_monte_carlo_is_deterministic_for_a_seed` | 同 seed 必须完全复现 | 用了全局 `random` 或 `time` 就挂 |

**先说复杂度再动手**：

> "Ranks only span 1..9, so the search space isn't the cards, it's the ranks. There are only 84 non-decreasing rank triples summing to 15 — I'll count with a frequency table and binomials instead of enumerating cards."

### 计数：频次表 + 组合数

```python
freq = Counter(c.rank for c in cards)
for r1, r2, r3 in all_triplet_ranks():        # r1 <= r2 <= r3, 共 84 组
    if r1 == r2 == r3: total += comb(freq[r1], 3)
    elif r1 == r2:     total += comb(freq[r1], 2) * freq[r3]
    elif r2 == r3:     total += freq[r1] * comb(freq[r2], 2)
    else:              total += freq[r1] * freq[r2] * freq[r3]
```

O(n + 84)，与组合数无关。**注意重复点数的三种退化情况** —— 直接写 `f1*f2*f3` 在 `(5,5,5)` 上会算成 `f5³`，测试里 `count([5]*6) == 20` 就是抓这个的。

### 选牌：按点数分桶

```python
buckets = defaultdict(list)          # rank -> [cards]
for r1, r2, r3 in all_triplet_ranks():
    need = Counter((r1, r2, r3))
    if all(len(buckets[r]) >= n for r, n in need.items()):
        return tuple(card for r, n in need.items() for card in buckets[r][:n])
return None
```

关键收益：**"无解"的判定也只要 O(84)**，而不是枚举完所有组合才敢返回 `None`。这正是对抗样例攻击的点。

### Monte Carlo

一个 `random.Random(seed)` 从头用到尾，顺序固定 → 同 seed 完全复现。**别**在模块级调 `random.*`，**别**用全局 rng。

---

## 5. 收尾：你自己该补的测试

跑绿之后主动加（说出来）：

- differential test：`count_triplets_sum_15` vs 暴力 `combinations` 在随机小输入上必须一致（测试里已有，指出它就是你验证优化正确性的手段）
- 空桌 / 只有 2 张 / 全同点数
- `FastStrategy` 与 `GreedyStrategy` 在同一小桌面上都必须终局无解（两者得分**不一定相等** —— 贪心顺序不同，这点值得主动讲）

---

## 6. Prompt 序列

见 [`prompts.md`](./prompts.md)。原则：Phase 1 先让 AI 解释根因（不写码），Phase 2 写死签名，Phase 3 先报瓶颈再让它改。
