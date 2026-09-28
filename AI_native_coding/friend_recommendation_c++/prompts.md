# Friend Recommendation (C++) — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read social_graph.h, recommenders.h and tests/test_friend_recommendation.cpp.
Do NOT write implementation code.

List:
1. The five validity rules a recommendation list must satisfy, and which of
   them the current is_valid_recommendations actually enforces
2. The ranking contract of recommend_baseline: score, eligibility threshold,
   tie-break, and what happens for an unknown user or k <= 0
3. What evaluate() divides by for precision and for recall, and why `holdout`
   is a std::map rather than an unordered_map
4. Why friends() returns a const reference instead of a copy
5. What the Phase 4 stress shapes are and what each one is punishing
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Two Phase-1 failures:
  test_rejects_existing_friend
  test_rejects_unknown_candidate

Explain each root cause in one sentence, naming the exact line in
recommenders.cpp. The graph is IRREFLEXIVE (add_friendship throws on a
self-loop) -- use that fact in the first explanation.
Do NOT propose code yet.
```

期望它说出：`graph.friends(candidate).count(candidate)` 查的是候选自己的好友集，非自反图下永远为 0，该分支是死代码；规则 5 完全没有对应的 check。

---

## Prompt 2 — Phase 1 修复

```
Task: fix is_valid_recommendations in recommenders.cpp so all five documented
rules are enforced.
Constraints:
- Rule 3 must test membership in the SOURCE user's friend set
- Rule 5 must reject candidate ids that are not in the graph
- Keep rules 3 and 5 as separate checks; they overlap but are not equivalent
- Hoist graph.friends(user) into a `const FriendSet&` outside the loop; do NOT
  copy it
- Signature unchanged; do not implement the other three functions
Return only the changed function.
```

跑：`make test`

---

## Prompt 3 — Phase 2 baseline

```
Task: implement recommend_baseline(graph, user, k) in recommenders.cpp.
Constraints:
- Score of candidate c = |friends(user) & friends(c)|
- Only candidates with score >= 1 are eligible
- Exclude the user and everyone already in friends(user)
- Sort by score DESCENDING, then user id ASCENDING; return the first k
- std::sort is NOT stable, so the tie-break must live in the comparator --
  do not rely on insertion order
- Scores are std::size_t (unsigned): do NOT negate them to reverse the sort,
  that wraps around. Write the comparator explicitly
- When intersecting two friend sets, iterate the SMALLER one
- Unknown user or k <= 0 -> {}
- The output must satisfy is_valid_recommendations for every user
- Scanning graph.users() is fine at this stage
- Do NOT touch evaluate or recommend_fast
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

## Prompt 5 — Phase 3 评估

```
Task: implement evaluate(graph, recommend_fn, holdout, k) in recommenders.cpp.
Constraints:
- For each user in holdout, call recommend_fn(graph, user, k) and count
  hits = |set(recs) & set(holdout[user])|
- precision contribution = double(hits) / k   <-- divide by k, NOT by recs.size()
  and cast to double first; integer division would truncate to 0
- recall contribution = double(hits) / holdout[user].size(); 0.0 if empty
- coverage contribution = 1.0 if recs is non-empty else 0.0
- Return the mean of each
- Empty holdout -> all three are 0.0; k <= 0 throws std::invalid_argument
- Iterate the std::map in its natural (sorted) order so the floating-point
  accumulation is reproducible run to run
- recommend_fn stays a std::function parameter so any recommender can be scored
Return only the function body.
```

跑：`make test`

---

## Prompt 6 — Phase 4（先说瓶颈，再让它写）

```
The stress set is 40k users with average degree 4, queried 2000 times. The
baseline scores every user in the graph -- measured at 16.4 seconds. A candidate
can only score if it shares a friend with the user, so the only nodes worth
visiting are friends-of-friends, about 16 of them.

Key identity: f in friends(user) and c in friends(f)  <=>  f in
friends(user) & friends(c). So counting how many of the user's friends point at
c IS the mutual friend count; no set intersection is needed.

Task: implement recommend_fast as a 2-hop count.
Constraints:
- std::unordered_map<std::string, std::size_t> counts; ++counts[candidate]
  relies on value-initialisation giving 0 -- say so in a comment
- Skip the user themselves and anyone already in friends(user)
- Reuse the same comparator as recommend_baseline so the tie-break is identical
- Must return exactly the same vector as recommend_baseline
- Keep recommend_baseline as-is so I can differential-test them
- Do not copy any FriendSet
Return only the function body.
```

跑：`make test`，然后

```bash
make clean
make test IMPL=solution CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"
```

---

## Prompt 7 — 复杂度与失效边界（只问，不写）

```
State the complexity of recommend_fast in terms of the user's degree d and the
degrees of their friends, versus recommend_baseline in terms of N. Then tell me
the graph shape where 2-hop expansion is WORSE than the full scan, and two ways
production systems deal with it.
Do NOT change code.
```

期望答案：`O(Σ deg(f) for f in friends(user))` vs `O(N · deg)`；**一个超高度数的名人节点**就能让 2-hop 退化 —— 8000 好友的节点让单次查询走 8000 步；两条出路是按度数截断，或用 **Adamic-Adar**（`1/log(deg(f))`）加权。

---

## Prompt 8 — 引用有效性审查（C++ 版专属）

```
Review recommenders.cpp for reference and iterator lifetime issues only:
- friends() returns a const reference into the graph's internal map. Under what
  operations does that reference become dangling?
- any reference or iterator held across a container mutation
- any place a FriendSet is copied where a const& would do
Report findings with file:line. Do NOT change code.
```

期望它区分：`unordered_map` 的 rehash **不会**让已有元素的引用失效（只让迭代器失效），但 `erase` 会。图在推荐过程中不变，所以当前用法安全 —— **能说清这个区别就是 C++ 的加分点**。

---

## Prompt 9 — 收尾 review

```
Review recommenders.cpp against the tests only.
Call out dead code, any rule in the is_valid_recommendations contract that has
no enforcement, any integer division that should be floating point, and any
behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "If we optimised for recommendation quality rather than matching the baseline
> exactly, I'd switch the score from raw mutual-friend count to **Adamic-Adar**:
> weight each shared friend by `1/log(deg(f))`, so a mutual connection through a
> celebrity with 8k friends counts for far less than one through someone with
> five. That also bounds the 2-hop walk, because I can skip neighbours above a
> degree threshold — their contribution rounds to nothing anyway. Different
> objective, same traversal. And it changes the score type from size_t to
> double, which means the tie-break comparator needs an epsilon or a stable
> secondary key — worth deciding before writing it, not after."
