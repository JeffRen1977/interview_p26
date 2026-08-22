# Meta AI-Enabled Coding — 60 分钟实战 Playbook

这一轮不是 LeetCode，也不是"让 AI 帮你写完"。它是一个**单项目、多 Checkpoint（3–4 个阶段）的工程模拟**：给你一个 2–5 文件的陌生小工程 + 一套预置单元测试，看你在 60 分钟里能不能

1. **快速定位**：在别人的代码里读出接口契约，指出 bug 在哪、为什么；
2. **AI 协同**：用结构化指令让模型只改该改的地方，而不是让它重写世界；
3. **测试防御**：每一步都用测试证明，主动补边界用例，不靠"看起来对"。

> 通过线：**至少完整跑通 Phase 1–3**。Phase 1 卡住基本等于 No Hire；Phase 3 只做到"能跑但 TLE"通常是 Lean Hire；Phase 3 主动识别瓶颈并重构成功 = Strong Hire。

---

## 0. 60 分钟时间盘（先背下来）

| 时刻 | 动作 | 产出 |
|------|------|------|
| 00:00–00:03 | **环境侦察**：`ls -R`、跑一次现有测试、看它怎么红的 | 知道 baseline 有几个 fail |
| 00:03–00:10 | **读契约**：读 model / 数据结构 / 测试文件，口述接口 | 一句话说清 public API |
| 00:10–00:18 | **Phase 1 修 bug** | Baseline 测试全绿 |
| 00:18–00:35 | **Phase 2 新功能** | 中等规模功能测试绿 |
| 00:35–00:52 | **Phase 3 规模/边界优化** | 压力测试 + 边界用例绿 |
| 00:52–00:58 | **自查 + 补边界测试 + 复述取舍** | 你自己写的 2–3 个 test |
| 00:58–01:00 | 提问 / 收尾 | — |

硬规则：**每个 Phase 结束必须跑测试**。没跑测试就往下走，是这一轮最常见的失分点。

---

## 1. 开场三分钟：先侦察，别打字

面试官放出仓库后，第一件事不是提示词，是这四条命令（按项目语言换）：

```bash
ls -R . | head -50                 # 结构
sed -n 1,60p tests/test_*.py       # 测试 = 真正的 spec
python3 -m unittest discover -v    # baseline 到底红在哪
git log --oneline | head           # 有时能看出 bug 是哪次引入的
```

同时**开口说话**（面试官在打分表上记的就是这些）：

> "Let me first run the existing tests to see the baseline, then read the test file — the tests are the spec here, so I want to know what contract I'm being held to before I touch any implementation."

侦察时要在脑子里回答三个问题：

1. **Public API 是什么**（函数签名 / 返回类型 / 谁调用谁）——这是后面所有 prompt 的"不许改"清单；
2. **数据从哪来**（时间戳、随机种子、输入规模是测试注入的还是代码里生成的）；
3. **测试分了几组**（类名 / 文件名往往就是 Phase 1/2/3 的路线图）。

---

## 2. 每个 Checkpoint 的六步循环（RNCPRR）

不管哪个 Phase，都跑同一个循环。这是这一轮的"肌肉记忆"：

| 步骤 | 做什么 | 说什么（talk track） |
|------|--------|----------------------|
| **R**ead | 只读**相关的那一两个函数**，不通读全仓 | "The failure is in `is_valid_move`, so I'll read that and the Table API it depends on." |
| **N**arrate | **先口述根因再动手**——这是 Phase 1 的核心分 | "The membership check compares ranks, not card objects, so an off-table card with a matching rank passes, and the same card can be counted three times." |
| **C**ontract | 写下输入/输出/不许改的东西 | "Signature stays `is_valid_move(table, cards) -> bool`; I'm not touching `Card` or `Table`." |
| **P**rompt | 给 AI 结构化指令（模板见 §4） | （见下） |
| **R**un | **你**跑测试，不是 AI 说"应该过了" | "Let me run the Stage-1 tests." |
| **R**eview | 逐行看 diff，删掉多余的东西 | "It also added a `deepcopy` I don't need — removing it, the store already copies." |

**Narrate 那一步千万别省。** 面试官区分"会用 AI"和"被 AI 牵着走"，靠的就是你有没有在 prompt 之前说出根因。

---

## 3. 三个 Phase 各自的打法

### Phase 1 — 架构理解与 Bug 修复（目标 8–12 分钟）

**考点**：能不能在陌生代码里靠 traceback 精准定位，而不是把整个仓库丢给 AI。

打法：

1. 跑测试 → **读 traceback 最下面三行**，定位到文件 + 行号；
2. 只打开那个函数，找"契约违反点"（典型：集合/列表用错、比较对象 vs 比较字段、缺 `visited`、行列下标写反、边界用 `<=` 还是 `<`）；
3. **口述根因**，再让 AI 做**最小修复**；
4. 跑测试；顺手加一个"证明这个 bug 不会回来"的用例。

典型 bug 家族（记住这五个，覆盖大部分题）：

| 家族 | 症状 | 一眼判据 |
|------|------|----------|
| 身份 vs 值比较 | 同一元素被重复消费 | `x in list` 用了值相等，没做 multiset 计数 |
| 缺 visited | `RecursionError` / 挂死 | DFS/BFS 里找不到 `visited` 集合 |
| 行列写反 | 输出图形错位 | `grid[c][r]` 与 `grid[r][c]` 混用 |
| 边界闭合 | 差一个元素 | 过期/窗口判断用 `>=` 还是 `>` |
| 状态外泄 | 测试互相污染 | 可变默认参数 / 类变量当实例状态 |

**陷阱**：一上来 `"here's my whole repo, fix all failing tests"`。哪怕它真修好了，这个 Phase 你也拿不到分。

### Phase 2 — 新功能实现与 AI 协同（目标 15–18 分钟）

**考点**：能不能把一个算法需求翻译成**契约 + 策略**，让 AI 生成可直接集成的代码。

打法：

1. 先说清**算法选型**和理由（BFS vs DFS vs Dijkstra；贪心 vs 全搜索），一句话即可；
2. 用 §4 的"实现模板"下指令，**显式写死签名和禁改清单**；
3. 生成后**先读再跑**：重点看它有没有偷偷改公共接口、有没有引入新依赖、有没有把测试注入的时钟换成 `time.time()`；
4. 跑测试 → 只贴失败输出，让它"只修这个失败"。

**陷阱**：`"帮我写 Phase 2"`。模糊 prompt = AI 幻觉改公共接口 = 后面 Phase 3 连锁崩。

**保险动作**：Phase 2 开始前 `git add -A && git commit -m "phase1 green"`。AI 把文件改乱时，`git diff` / `git checkout --` 是你 10 秒回滚的后悔药，面试官也乐意看到这个习惯。

### Phase 3 — 规模优化与极端边界（目标 15–18 分钟）

**考点**：能不能**主动**识别瓶颈，而不是死等超时或让 AI 盲目重写。

打法（这是最能拉开差距的一段）：

1. 测试卡住/超时时，**先说复杂度**："The current solution is O(3^L) backtracking; with 5000 words that's why it hangs."
2. **给出替代方案和理由**，再让 AI 动手：频次表 / Trie / bitmask / 记忆化 / 分层 BFS；
3. 保持**旧实现不删**，新增 `*_fast` 函数并让两者在小输入上互测（differential testing）——这是 Senior 信号；
4. 补边界用例：空集、单元素、全相同、环路、不可达、超长输入。

优化决策树（背这张表）：

| 症状 | 换什么 | 复杂度变化 |
|------|--------|-----------|
| 三数之和 / 组合计数在大集合上炸 | 值域小 → **频次表**枚举值组合 + 组合数 | O(n³) → O(V³ + n) |
| 子集搜索、词表选取 | **bitmask + 上界剪枝**（当前 + 剩余可能 ≤ best 就剪） | O(2ⁿ·n) → 实测近线性 |
| 大量字符串前缀共享 | **Trie** 剪枝 | 每次 O(L) 而非 O(N·L) |
| 网格最短路 + 必经点/钥匙 | **状态压缩 BFS** `(r, c, mask)` | 分段 BFS 的组合爆炸 → O(R·C·2ᵏ) |
| 无权最短路却在用 DFS | **BFS** | 指数 → O(V+E) |
| 反复重算同一子问题 | 记忆化 / DP | 指数 → 多项式 |

**陷阱**：`"it's too slow, rewrite it"`。AI 会给你一个换汤不换药的版本，浪费 5 分钟。

### Phase 4（如果有）— 通常是并发、持久化或对抗输入

来得及就做，做不完也要**口头说清方案**："With two replicas sharing the store, the get-then-set is a read-modify-write race; in production I'd move it into a Redis Lua script." 说清楚也能拿分。

---

## 4. 高分 Prompt 模板（背 5 个就够）

原则：**Task + Constraints + Do-NOT + Output-format**。四段缺一不可。

### 模板 A — 抽 spec（不写代码）

```
Read <files>. Do NOT write implementation code.
List:
1. The public API I must implement (exact signatures and return types)
2. The invariant each test class is asserting
3. Edge cases implied by the tests (empty input, duplicates, boundary values)
4. Anything the tests do NOT require, so we don't build it
```

### 模板 B — 定位 bug（不写代码）

```
Test <name> fails with:
<paste traceback>

Explain the root cause in <function> in terms of the contract in <file>.
Do NOT propose a fix yet. Tell me which single line is wrong and why.
```

### 模板 C — 实现（用户给的模板，扩展版）

```
Task: Implement the `find_triplets_sum_15` function in `strategy.py`.
Constraints:
- Input: List[Card] (cards on current table)
- Output: List[Tuple[Card, Card, Card]] (all non-overlapping valid sets)
- Approach: Use a frequency map / two-pointer technique to achieve O(N^2) or better.
- Do NOT modify any existing class structures or method signatures in Card or Table.
- Do NOT add new dependencies or import time/random.
- Return ONLY the helper function code snippet.
```

### 模板 D — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code,
do not rename anything, do not touch other stages.
<paste unittest output>
```

### 模板 E — 优化（先说瓶颈再让它写）

```
`max_unique_length` is O(2^n) with set copies and times out on the 24-word case.
Rewrite it as `max_unique_length_fast` using:
- one 26-bit int per word (bitwise OR to test compatibility)
- DFS with an upper-bound prune: current_len + remaining_len <= best -> cut
Keep the original function untouched so I can differential-test them.
Same signature: (words: List[str]) -> int
```

### 模板 F — 收尾 Review（跑绿之后）

```
Review <file> against the tests only. Call out:
dead code you added, hidden O(n^2) paths, and any behaviour not covered by a test.
Do NOT change code unless I ask.
```

---

## 5. 反模式清单（面试官的扣分项）

| 反模式 | 为什么扣分 | 替换动作 |
|--------|-----------|----------|
| 一上来把全仓丢给 AI | 看不出你会不会读代码 | 先跑测试 + 口述根因 |
| `"帮我写 Phase 2"` | 幻觉改公共接口 | 写死签名 + Do-NOT 清单 |
| AI 说"已修复"就往下走 | 你在替 AI 背锅 | 每步自己跑测试 |
| 超时了干等 | 没有性能直觉 | 先报复杂度，再选替代结构 |
| 让 AI 重写整个文件 | diff 失控、回滚困难 | 只让它改一个函数，`git diff` 逐行看 |
| 全程沉默打字 | 拿不到"沟通/主导力"分 | 每个动作前一句话说意图 |
| 改测试让它变绿 | 直接 No Hire | 除非面试官明说测试有 bug，且你先口述证据 |
| 加 `time.sleep` / 随机数 | 测试不稳定 | 用测试注入的时钟和 seed |

---

## 6. 测试防御：你自己要补的边界

Phase 3 结束后，主动加 2–3 个测试（**别忘了说 "let me add a couple of edge cases the suite doesn't cover"**）。通用清单：

- **空 / 单元素**：空表、空词表、1×1 网格
- **全相同**：所有牌同点数、所有词同字符
- **不可达 / 无解**：终点被墙围死、没有和为 15 的组合 → 必须返回 `None`/`[]` 而不是抛异常
- **起点 == 终点**：路径长度为 1 还是 0，说清楚你的定义
- **边界闭合**：恰好等于窗口/上限的那个值
- **不变量**：任何一张牌不被消费两次；路径每一步都相邻且不穿墙
- **differential test**：慢的正确实现 vs 快的实现，在随机小输入上结果必须一致

最后一条最值钱——它同时证明了"新算法是对的"和"我知道怎么验证优化"。

---

## 7. 三道真实样题 → 本目录的三个可跑工程

题目原文与考点拆解见 [`question_bank.md`](./question_bank.md)；下面三个目录是**可以直接练**的复刻工程（Phase 1 的 bug 是真的埋在代码里的）：

| 样题 | 目录 | 练什么 |
|------|------|--------|
| Card Game（和为 15） | [`card_game/`](./card_game/) | multiset 校验 bug → 贪心结算 → 频次表加速 + 蒙特卡洛 |
| Maze Solver | [`maze_solver/`](./maze_solver/) | 行列写反 + 缺 visited → BFS 最短路 → Bitmask BFS 收钥匙 |
| Max Unique Characters | [`max_unique_chars/`](./max_unique_chars/) | sanitize 大小写 bug → 回溯 → bitmask + 剪枝 |
| RateLimiter Engine | [`ratelimiter_engine/`](./ratelimiter_engine/) | 6 阶段：滑窗 / 多租户 / 热更新 / 降级 |

每个目录都是：

```
<sample>/
├── README.md      # 题面、契约、分阶段打法、参考算法
├── prompts.md     # 逐阶段可复制的 prompt
├── project/       # 面试开局状态：bug 埋着、Phase 2/3 是 stub
├── solution/      # 参考实现
└── tests/         # spec（三个 Phase 的测试类）
```

跑法（在样题目录下）：

```bash
python3 -m unittest discover -s tests -v          # 测 project/（开局：会红）
AINC_IMPL=solution python3 -m unittest discover -s tests -v   # 测 solution/（应全绿）
```

---

## 8. 两天冲刺计划

**Day 1**

- 上午：读本文 + `question_bank.md`，把 §4 的五个 prompt 模板抄一遍到自己的备忘里
- 下午：`card_game/` 完整计时跑一遍（60 分钟闹钟，全程说英文）
- 晚上：对照 `solution/` 复盘，记下你在哪个 Phase 超时

**Day 2**

- 上午：`maze_solver/`（重点练 Phase 3 的 bitmask 状态设计）
- 下午：`max_unique_chars/`（重点练"先报复杂度再优化"）
- 晚上：`ratelimiter_engine/`（练非算法型的工程题：降级、热更新）

练的时候强制两条规则：**(1) 每个 Phase 结束跑测试并 commit；(2) 全程出声解释**。这两条就是这一轮的评分表。

---

## 9. 临场话术速查（英文）

| 时机 | 说 |
|------|-----|
| 开场 | "Let me run the tests first — the test file is the spec, I want the contract before I write anything." |
| 定位 bug | "The root cause is X: it compares ranks instead of card objects, so a card that isn't on the table passes the check." |
| 下 prompt 前 | "I'll ask the model for just this helper, with the signature pinned, so nothing else in the module moves." |
| Review AI 输出 | "It added a deep copy I don't need — the store already copies on read, so I'll drop that." |
| 遇到超时 | "This is exponential in the number of words. Before optimizing I'll confirm the bottleneck is the subset search, not the parsing." |
| 换算法 | "I'll keep the naive version and add a bitmask version, then differential-test them on random small inputs." |
| 收尾 | "Tests are green. Let me add two edge cases the suite doesn't cover: empty input and start == end." |
| 没做完 | "I didn't finish the concurrency stage, but the fix is to move the read-modify-write into a single atomic script — here's how I'd do it." |
