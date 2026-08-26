# Friend Recommendation — 社交图 People-You-May-Know

> Meta AI-Enabled Coding 轮换池里的图论题（难度 Easy，但有 4 个 Checkpoint）。`project/` 是**面试开局状态**。

```bash
cd friend_recommendation
python3 -m unittest discover -s tests -v                       # 测 project/ → 2 FAIL + 17 ERROR / 24
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 24 OK
python3 -m unittest discover -s tests -k Phase2 -v             # 只跑当前 Phase
```

---

## 1. 题面与契约

一张无向社交图，给某个用户推荐"你可能认识的人"。四个阶段：**修校验 bug → 基础排序 → 离线评估 → 扛住规模**。

```
project/
├── social_graph.py   # SocialGraph（已给，不要改）
└── recommenders.py   # is_valid_recommendations  （Phase 1 bug ×2）
                      # recommend_baseline        （Phase 2 桩）
                      # evaluate                  （Phase 3 桩）
                      # recommend_fast            （Phase 4 桩）
tests/test_friend_recommendation.py
    # Phase1Validation / Phase2Baseline / Phase3Evaluate / Phase4Scale
```

```python
# 已给
graph.has_user(uid) -> bool
graph.users()       -> List[str]        # 排序后的全部 id
graph.friends(uid)  -> AbstractSet[str] # 只读视图，未知用户返回空集
graph.degree(uid)   -> int

# 要写
is_valid_recommendations(graph, user, candidates) -> bool
recommend_baseline(graph, user, k=5) -> List[str]
evaluate(graph, recommend_fn, holdout, k=3) -> {"precision","recall","coverage"}
recommend_fast(graph, user, k=5) -> List[str]
```

**推荐列表的五条合法性规则**（Phase 1 的契约，后面三个 Phase 都靠它兜底）：

| # | 规则 |
|---|------|
| 1 | `user` 自己必须在图里 |
| 2 | 不能把 `user` 推荐给他自己 |
| 3 | **已经是 `user` 好友的人不能出现在推荐里** |
| 4 | 候选不能重复 |
| 5 | **候选必须是图里存在的 id** |

友谊是**对称且非自反**的（`add_friendship` 拒绝自环）。空候选列表永远合法。

**排序契约**：候选分数 = `|friends(user) ∩ friends(c)|`（共同好友数），只有分数 ≥ 1 的才有资格；**按分数降序，id 升序做 tie-break**，取前 k。未知用户或 `k <= 0` 返回 `[]`。

---

## 2. Phase 1 — 校验函数的两个 bug（目标 6–8 分钟）

```
FAIL test_rejects_existing_friend
FAIL test_rejects_unknown_candidate
```

### Bug #1：查错了一边的好友集

```python
if candidate in graph.friends(candidate):   # ← 永远是 False
    return False
```

`graph.friends(candidate)` 是**候选自己的**好友集，而没人是自己的好友（图非自反），所以这个分支**永远不触发**。规则 3 看起来实现了，实际是死代码。应该是 `graph.friends(user)`。

**口述**：

> "This line looks like it excludes existing friends, but it asks the candidate whether it is its own friend. The graph is irreflexive, so that branch can never fire — rule 3 is effectively unimplemented. It should test membership in the *source* user's friend set: `candidate in graph.friends(user)`."

顺手把 `own = graph.friends(user)` 提到循环外 —— 这是 O(1) 的字典查，但每轮重复取没有意义，提出来后意图也更清楚。

### Bug #2：规则 5 从来没写

docstring 列了五条，代码里只有四条的位置，`graph.has_user(candidate)` 一次都没被调用。

**口述**：

> "Rule 5 in the docstring has no corresponding check at all — an id that isn't in the graph passes validation today. I'll add `if not graph.has_user(candidate): return False`."

> **这里有个可以主动提的点**：规则 3 和规则 5 有重叠但不等价 —— 好友一定存在于图里，但存在于图里的不一定是好友。两个 check 都要留着。面试官喜欢听你区分这个。

**这一步不要用 AI 生成修复**。两处都是一行，自己改完跑测试，把根因说清楚，比 prompt 一轮更快也更值钱。

---

## 3. Phase 2 — 共同好友排序（扫描即可，目标 10–12 分钟）

```python
def recommend_baseline(graph, user, k=5):
    if k <= 0 or not graph.has_user(user):
        return []
    own = graph.friends(user)
    scored = []
    for other in graph.users():
        if other == user or other in own:
            continue
        mutual = len(own & graph.friends(other))
        if mutual:
            scored.append((-mutual, other))
    scored.sort()
    return [uid for _, uid in scored[:k]]
```

测试钉死的四件事：

- **排序**：`(-mutual, id)` 的元组排序天然就是"分数降序 + id 升序"，不用写 `key=`；
- **排除自己和已有好友**：`test_excludes_self_and_existing_friends` 会检查 dave 的结果里没有 bob/carol/erin；
- **`k` 的边界**：`k=0` 返回 `[]`，`k` 大于候选数就全给；
- **自洽**：`test_output_passes_the_phase1_validator` 会把每个用户的输出再喂回 Phase 1 的校验器。**Phase 1 修对了这条才会绿** —— 这是题目故意设的连锁。

**陷阱**：`mutual` 为 0 的人不能进结果。如果你写成"全部打分再排序取前 k"，一个没有任何共同好友的陌生人也会被推上来（`test_no_mutual_friends_returns_empty` 抓这个）。

扫描是 Phase 2 的正确解。**不要在这一阶段就写 2-hop** —— 那是 Phase 4 的考点，提前做等于没给面试官看你识别瓶颈的过程。

---

## 4. Phase 3 — 离线评估（目标 8–10 分钟）

推荐系统面试的"半个系统设计"环节：把一部分真实好友关系藏起来（holdout），看推荐器能不能猜回来。

```python
def evaluate(graph, recommend_fn, holdout, k=3):
    if k <= 0:
        raise ValueError("k must be >= 1")
    users = sorted(holdout)
    if not users:
        return {"precision": 0.0, "recall": 0.0, "coverage": 0.0}
    precision = recall = coverage = 0.0
    for user in users:
        actual = set(holdout[user])
        recs = recommend_fn(graph, user, k)
        hits = len(set(recs) & actual)
        precision += hits / k
        recall += hits / len(actual) if actual else 0.0
        coverage += 1.0 if recs else 0.0
    n = float(len(users))
    return {"precision": precision / n, "recall": recall / n, "coverage": coverage / n}
```

三个指标，每个都有一条测试专门钉它：

| 指标 | 定义 | 坑 |
|------|------|-----|
| precision@k | `hits / k` | **分母是 k，不是 `len(recs)`**。只返回 1 个且命中，k=4 时是 0.25 不是 1.0 |
| recall@k | `hits / len(holdout[user])` | holdout 为空时记 0.0，不能除零 |
| coverage | 返回了至少一个推荐的用户占比 | 衡量"推荐器对多少人有话说"，跟命中率是两回事 |

**口述**（面试官在等这句）：

> "Precision divides by k on purpose: a recommender that returns one safe pick and stays silent otherwise shouldn't score the same as one that fills all k slots correctly. That's also why coverage is a separate metric — it catches the recommender that's precise only because it almost never fires."

`recommend_fn` 是参数不是写死的函数 —— 这样 Phase 4 的 `recommend_fast` 和任何加权变体都能直接进同一个评估管道。**这个设计意图要说出来**。

---

## 5. Phase 4 — 只走 2-hop（目标 12–15 分钟）

| 压力测试 | 形状 | 为什么 Phase 2 会死 |
|----------|------|---------------------|
| `test_large_sparse_graph_many_queries` | 4 万用户，平均 4 个好友，查 2000 次 | 每次 O(N·deg) 扫全图 ≈ **14 秒**；预算 1.5 秒 |
| `test_same_answers_as_the_baseline` | 400 点随机图，120 个用户逐一对比 | differential：快版和慢版必须**逐字节相同**，包括 tie-break |
| `test_high_degree_hub_is_handled` | 一个 8000 好友的名人 + 星形图 | 高度数节点下答案仍要与 baseline 一致 |

**先说瓶颈再动手**：

> "The baseline scores every user in the graph, but a candidate can only have a non-zero score if it shares a friend with me — that means it's exactly two hops away. In a 40k graph with average degree 4 that's ~16 nodes instead of 40k. So I'll walk friends-of-friends and count, instead of scanning and intersecting."

关键恒等式（写代码前把它说出来，这是这道题的**全部**内容）：

```
f ∈ friends(user) 且 c ∈ friends(f)   ⟺   f ∈ friends(user) ∩ friends(c)
```

也就是说：**"有多少个我的好友指向 c" 本身就是共同好友数**，不需要再做集合求交。

```python
def recommend_fast(graph, user, k=5):
    if k <= 0 or not graph.has_user(user):
        return []
    own = graph.friends(user)
    counts = Counter()
    for friend in own:
        for candidate in graph.friends(friend):
            if candidate == user or candidate in own:
                continue
            counts[candidate] += 1
    ranked = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
    return [uid for uid, _ in ranked[:k]]
```

复杂度从 `O(N · deg)` 变成 `O(Σ deg(f) for f in friends(user))` —— 稀疏图上是常数级。

### 一定要主动说的反例（这题的加分点全在这里）

> "2-hop expansion is not universally better. Its cost is the sum of my friends' degrees, so **one celebrity friend dominates everything**: if I follow an account with 8k friends, my 2-hop walk touches 8k nodes while a scan of a small graph might touch fewer. Production systems cap it — skip neighbours above a degree threshold, or weight by **Adamic-Adar** (`1/log(deg(f))`) so a shared celebrity counts for far less than a shared close friend. I'm not doing it here because the tests require identical output to the baseline, but that's the knob I'd turn if we optimised for quality instead."

`test_high_degree_hub_is_handled` 就是把这个形状摆在你面前等你说这段话。**能说出 Adamic-Adar 这个词，这一轮的图论部分就满分了。**

---

## 6. 收尾：你自己该补的测试

- differential：`recommend_fast` vs `recommend_baseline` 换个 seed 再跑一组
- 自环：`add_friendship("a", "a")` 应该 `ValueError`（已给的图实现里有，确认一下）
- 孤立节点：`degree == 0` 的用户返回 `[]`
- 重复调用 `add_friendship` 幂等（集合天然去重）
- 全连通小图：所有人互为好友时，每个人的推荐都是 `[]`（没人可推）
- `k` 大于候选总数时不 padding、不报错

---

## 7. Prompt 序列

见 [`prompts.md`](./prompts.md)。
