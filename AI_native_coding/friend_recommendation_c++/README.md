# Friend Recommendation (C++) — 社交图 People-You-May-Know

> Python 版 [`../friend_recommendation/`](../friend_recommendation/) 的 C++17 复刻。契约、Phase 划分、埋的 bug 完全一致。`project/` 是**面试开局状态**。

```bash
cd friend_recommendation_c++
make test                     # 测 project/ → 2 FAIL + 18 ERROR / 25
make test IMPL=solution       # 测 solution/ → 25 OK
make clean
```

---

## 1. 题面与契约

一张无向社交图，给某个用户推荐"你可能认识的人"。四个阶段：**修校验 bug → 基础排序 → 离线评估 → 扛住规模**。

```
project/
├── social_graph.h/.cpp  # SocialGraph（已给，不要改）
└── recommenders.h/.cpp  # is_valid_recommendations  ← Phase 1 bug ×2
                         # recommend_baseline        （Phase 2 桩）
                         # evaluate                  （Phase 3 桩）
                         # recommend_fast            （Phase 4 桩）
tests/test_friend_recommendation.cpp
    # Phase1Validation / Phase2Baseline / Phase3Evaluate / Phase4Scale
```

```cpp
// 已给
bool                     has_user(const std::string&) const;
std::vector<std::string> users() const;                    // 排序后的全部 id
const FriendSet&         friends(const std::string&) const; // 只读引用，未知用户返回空集
std::size_t              degree(const std::string&) const;

// 要写
bool is_valid_recommendations(const SocialGraph&, const std::string& user,
                              const std::vector<std::string>& candidates);
std::vector<std::string> recommend_baseline(const SocialGraph&, const std::string&, int k = 5);
Metrics evaluate(const SocialGraph&, const Recommender&,
                 const std::map<std::string, std::vector<std::string>>& holdout, int k = 3);
std::vector<std::string> recommend_fast(const SocialGraph&, const std::string&, int k = 5);
```

**推荐列表的五条合法性规则**（Phase 1 的契约，后面三个 Phase 都靠它兜底）：

| # | 规则 |
|---|------|
| 1 | `user` 自己必须在图里 |
| 2 | 不能把 `user` 推荐给他自己 |
| 3 | **已经是 `user` 好友的人不能出现在推荐里** |
| 4 | 候选不能重复 |
| 5 | **候选必须是图里存在的 id** |

友谊**对称且非自反**（`add_friendship` 拒绝自环）。空候选列表永远合法。

**排序契约**：分数 = `|friends(user) ∩ friends(c)|`，只有 ≥ 1 才有资格；**分数降序、id 升序 tie-break**，取前 k。未知用户或 `k <= 0` 返回 `{}`。

> **C++ 版的一个 API 决策值得说**：`friends()` 返回 `const FriendSet&` 而不是拷贝。40k 用户 × 2000 次查询的压力集下，每次查询都拷一份邻接集合会把 Phase 4 的优化全部吃掉。注释里写明"只读，别改"，用 `const` 引用兑现它 —— **这是 C++ 里"契约写进类型"最直接的一种形式**。

---

## 2. Phase 1 — 校验函数的两个 bug（目标 6–8 分钟）

```
FAIL test_rejects_existing_friend
FAIL test_rejects_unknown_candidate
```

### Bug #1：查错了一边的好友集

```cpp
if (graph.friends(candidate).count(candidate)) return false;   // ← 永远是 false
```

`graph.friends(candidate)` 是**候选自己的**好友集，而没人是自己的好友（图非自反，`add_friendship` 连自环都拒绝），所以这个分支**永远不触发**。规则 3 看起来实现了，实际是死代码。

应该是 `graph.friends(user).count(candidate)`。顺手把它提到循环外：

```cpp
const FriendSet& own = graph.friends(user);   // 只查一次哈希表，而且意图更清楚
```

**口述**：

> "This line looks like it excludes existing friends, but it asks the candidate whether it is its own friend. The graph is irreflexive, so that branch can never fire — rule 3 is effectively unimplemented. It should test membership in the *source* user's friend set."

### Bug #2：规则 5 从来没写

头文件的契约列了五条，代码里只有四条的位置，`graph.has_user(candidate)` 一次都没被调用。

**口述**：

> "Rule 5 in the header has no corresponding check at all — an id that isn't in the graph passes validation today."

> **主动提的点**：规则 3 和规则 5 有重叠但不等价 —— 好友一定存在于图里，但存在于图里的不一定是好友。两个 check 都要留着。

**这一步不要用 AI 生成修复**，两处都是一行。自己改完跑 `make test`，把根因说清楚，比 prompt 一轮更快也更值钱。

---

## 3. Phase 2 — 共同好友排序（扫描即可，目标 10–12 分钟）

```cpp
if (k <= 0 || !graph.has_user(user)) return {};
const FriendSet& own = graph.friends(user);
std::vector<std::pair<std::size_t, std::string>> scored;
for (const std::string& other : graph.users()) {
    if (other == user || own.count(other)) continue;
    const std::size_t mutual = mutual_count(own, graph.friends(other));
    if (mutual > 0) scored.emplace_back(mutual, other);
}
std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return a.first != b.first ? a.first > b.first : a.second < b.second;
});
```

**C++ 版的三个细节，每个都值得说：**

1. **比较器要显式写"分数降序 + id 升序"**。Python 里靠 `(-mutual, id)` 的元组排序天然就对；C++ 这里没有负数技巧可用（`std::size_t` 是无符号，取负会回绕），所以必须写 lambda。写 `a.first > b.first` 而不是 `-a.first < -b.first`。
2. **求交时遍历小的那个集合**：`mutual_count` 里先比大小再遍历，代价是 `O(min(|a|,|b|))` 而不是 `O(|a|)`。名人节点下这个差别很大。
3. **`std::sort` 不稳定**。所以 tie-break 必须写进比较器，不能指望"相等时保持原顺序"。`test_ties_break_by_user_id` 专抓这条 —— 如果你只按分数排序，`{adam, mia, zoe}` 的顺序会随实现而变。

测试钉死的四件事：排序、排除自己和已有好友、`k` 的边界（含 `k = -1`）、以及**自洽**：`test_output_passes_the_phase1_validator` 会把每个用户的输出再喂回 Phase 1 的校验器 —— **Phase 1 修对了这条才会绿**。

**陷阱**：`mutual == 0` 的人不能进结果。写成"全部打分再排序取前 k"，陌生人会被推上来（`test_no_mutual_friends_returns_empty` 抓这个）。

---

## 4. Phase 3 — 离线评估（目标 8–10 分钟）

推荐系统的"半个系统设计"：藏起一部分真实好友关系（holdout），看推荐器能不能猜回来。

```cpp
Metrics evaluate(const SocialGraph& graph, const Recommender& recommend_fn,
                 const std::map<std::string, std::vector<std::string>>& holdout, int k) {
    if (k <= 0) throw std::invalid_argument("k must be >= 1");
    if (holdout.empty()) return Metrics{};
    Metrics totals;
    for (const auto& [user, actual_list] : holdout) {      // std::map：按 key 有序，确定性
        const std::unordered_set<std::string> actual(actual_list.begin(), actual_list.end());
        const auto recs = recommend_fn(graph, user, k);
        std::size_t hits = 0;
        for (const auto& rec : recs) if (actual.count(rec)) ++hits;
        totals.precision += double(hits) / k;
        if (!actual.empty()) totals.recall += double(hits) / actual.size();
        if (!recs.empty())   totals.coverage += 1.0;
    }
    const double n = holdout.size();
    return {totals.precision / n, totals.recall / n, totals.coverage / n};
}
```

| 指标 | 定义 | 坑 |
|------|------|-----|
| precision@k | `hits / k` | **分母是 k，不是 `recs.size()`**。只返回 1 个且命中，k=4 时是 0.25 |
| recall@k | `hits / holdout[user].size()` | holdout 为空时记 0.0，不能除零 |
| coverage | 返回了至少一个推荐的用户占比 | 衡量"推荐器对多少人有话说"，跟命中率是两回事 |

**三个 C++ 细节：**

- **`holdout` 用 `std::map` 而不是 `unordered_map`**：有序遍历 → 浮点累加顺序固定 → 结果可复现。用 `unordered_map` 的话，同样的输入在不同运行/不同实现下累加顺序不同，最后一位有可能不一样。**这是"确定性"在浮点计算里的具体含义，值得主动说**。
- **`double(hits) / k`**，别写 `hits / k` —— 两个整数相除会截断成 0。
- **`Recommender` 是 `std::function`**，所以 `recommend_baseline`、`recommend_fast` 和任何加权变体都能进同一个评估管道。这个设计意图要说出来。

**口述**：

> "Precision divides by k on purpose: a recommender that returns one safe pick and stays silent otherwise shouldn't score the same as one that fills all k slots correctly. That's also why coverage is a separate metric — it catches the recommender that's precise only because it almost never fires."

---

## 5. Phase 4 — 只走 2-hop（目标 12–15 分钟）

| 压力测试 | 形状 | 实测 |
|----------|------|------|
| `test_large_sparse_graph_many_queries` | 4 万用户，平均 4 个好友，查 2000 次 | baseline **16.4 秒** / fast **0.003 秒**（预算 1.0 秒） |
| `test_same_answers_as_the_baseline` | 400 点随机图，120 个用户逐一对比 | differential：必须**逐字节相同**，包括 tie-break |
| `test_top_k_is_a_prefix_of_top_k_plus_one` | 300 点随机图，全部用户 | 变形关系：`k=3` 必须是 `k=5` 的前缀 |
| `test_high_degree_hub_is_handled` | 一个 8000 好友的名人 + 星形图 | 高度数节点下答案仍要与 baseline 一致 |

**先说瓶颈再动手**：

> "The baseline scores every user in the graph, but a candidate can only have a non-zero score if it shares a friend with me — that means it's exactly two hops away. In a 40k graph with average degree 4 that's ~16 nodes instead of 40k."

关键恒等式（写代码前把它说出来，这是这道题的**全部**内容）：

```
f ∈ friends(user) 且 c ∈ friends(f)   ⟺   f ∈ friends(user) ∩ friends(c)
```

**"有多少个我的好友指向 c" 本身就是共同好友数**，不需要再做集合求交。

```cpp
std::unordered_map<std::string, std::size_t> counts;
for (const std::string& friend_id : own) {
    for (const std::string& candidate : graph.friends(friend_id)) {
        if (candidate == user || own.count(candidate)) continue;
        ++counts[candidate];
    }
}
```

复杂度从 `O(N · deg)` 变成 `O(Σ deg(f) for f in friends(user))` —— 稀疏图上是常数级。

> **`++counts[candidate]` 为什么安全？** `operator[]` 在 key 不存在时**值初始化**，`std::size_t` 被初始化为 0，所以 `++` 得到 1。这是 C++ 里少数几处 `operator[]` 的默认构造正好是你想要的地方 —— 但如果 value 类型换成裸指针或者没有默认构造的类型，这行就不成立了。知道**为什么**它能用，比知道它能用重要。

### 一定要主动说的反例（这题的加分点全在这里）

> "2-hop expansion is not universally better. Its cost is the sum of my friends' degrees, so **one celebrity friend dominates everything**: if I follow an account with 8k friends, my 2-hop walk touches 8k nodes while a scan of a small graph might touch fewer. Production systems cap it — skip neighbours above a degree threshold, or weight by **Adamic-Adar** (`1/log(deg(f))`) so a shared celebrity counts for far less than a shared close friend. I'm not doing it here because the tests require identical output to the baseline, but that's the knob I'd turn if we optimised for quality instead."

`test_high_degree_hub_is_handled` 就是把这个形状摆在你面前等你说这段话 —— 注意它**只断言正确性，不断言速度**。**能说出 Adamic-Adar 这个词，这一轮的图论部分就满分了。**

---

## 6. 收尾：你自己该补的测试

- differential：换个 seed 再跑一组 `recommend_fast` vs `recommend_baseline`
- 自环：`add_friendship("a", "a")` 应该抛 `std::invalid_argument`（已给的实现里有，确认一下）
- 孤立节点：`degree == 0` 的用户返回 `{}`
- 重复调用 `add_friendship` 幂等（`unordered_set` 天然去重）
- 全连通小图：所有人互为好友时，每个人的推荐都是 `{}`
- `friends()` 返回引用后图被修改 —— 引用是否还有效？（`unordered_map` 的 rehash 不会让**值**的引用失效，但 `erase` 会。**主动说出这个区别**）
- 用 sanitizer 跑一遍：`make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"`

---

## 7. Prompt 序列

见 [`prompts.md`](./prompts.md)。
