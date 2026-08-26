# Friend Recommendation — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read social_graph.py, recommenders.py and tests/test_friend_recommendation.py.
Do NOT write implementation code.

List:
1. The exact validity rules a recommendation list must satisfy, and which of
   them the current is_valid_recommendations actually enforces
2. The ranking contract of recommend_baseline: score, eligibility threshold,
   tie-break, and what happens for an unknown user or k <= 0
3. What evaluate() divides by for precision and for recall
4. What the Phase 4 stress shapes are and what each one is punishing
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Two Phase-1 failures:
  test_rejects_existing_friend
  test_rejects_unknown_candidate

Explain each root cause in one sentence, naming the exact line in
recommenders.py. The graph is irreflexive (add_friendship rejects self-loops) —
use that fact in the first explanation.
Do NOT propose code yet.
```

期望它说出：`graph.friends(candidate)` 查的是候选自己的好友集，非自反图下永远为空 → 该分支是死代码；以及规则 5 完全没有对应的 check。

---

## Prompt 2 — Phase 1 修复

```
Task: fix is_valid_recommendations in recommenders.py so all five documented
rules are enforced.
Constraints:
- Rule 3 must test membership in the SOURCE user's friend set
- Rule 5 must reject candidate ids that are not in the graph
- Keep rules 3 and 5 as separate checks; they overlap but are not equivalent
- Hoist graph.friends(user) out of the loop
- Signature unchanged; do not implement the other three functions
Return only the changed function.
```

跑：`python3 -m unittest discover -s tests -k Phase1 -v`

---

## Prompt 3 — Phase 2 baseline

```
Task: implement recommend_baseline(graph, user, k=5) -> List[str].
Constraints:
- Score of candidate c = |friends(user) & friends(c)|
- Only candidates with score >= 1 are eligible
- Exclude the user and everyone already in friends(user)
- Sort by score descending, then user id ascending; return the first k
- Unknown user or k <= 0 -> []
- Scanning graph.users() is fine at this stage
- The output must satisfy is_valid_recommendations for every user
- Do NOT touch evaluate or recommend_fast
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase2 -v`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste unittest output>
```

---

## Prompt 5 — Phase 3 评估

```
Task: implement evaluate(graph, recommend_fn, holdout, k=3) in recommenders.py.
Constraints:
- For each user in holdout (sorted), call recommend_fn(graph, user, k) and count
  hits = |set(recs) & set(holdout[user])|
- precision contribution = hits / k   <-- divide by k, NOT by len(recs)
- recall contribution = hits / len(holdout[user]); 0.0 if the holdout set is empty
- coverage contribution = 1.0 if recs is non-empty else 0.0
- Return the mean of each as {"precision","recall","coverage"}
- Empty holdout -> all three are 0.0; k <= 0 raises ValueError
- recommend_fn stays a parameter so any recommender can be scored
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase3 -v`

---

## Prompt 6 — Phase 4（先说瓶颈，再让它写）

```
The stress set is 40k users with average degree 4, queried 2000 times. The
baseline scores every user in the graph, which is ~14 seconds. A candidate can
only score if it shares a friend with the user, so the only nodes worth
visiting are friends-of-friends — about 16 of them.

Key identity: f in friends(user) and c in friends(f)  <=>  f in friends(user) &
friends(c). So counting how many of the user's friends point at c IS the mutual
friend count; no set intersection is needed.

Task: implement recommend_fast using a Counter over the 2-hop neighbourhood.
Constraints:
- Must return exactly the same list as recommend_baseline, tie-break included
- Skip the user themselves and anyone already in friends(user)
- Keep recommend_baseline as-is so I can differential-test them
- Do not add dependencies
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase4 -v`

---

## Prompt 7 — 复杂度确认

```
State the complexity of recommend_fast in terms of the user's degree d, the
average degree of their friends, and the total user count N. Then tell me the
graph shape where 2-hop expansion is WORSE than the full scan.
Do NOT change code.
```

期望答案：`O(Σ deg(f) for f in friends(user))` vs baseline 的 `O(N·deg)`；当好友里有超高度数节点（名人）时，2-hop 会退化 —— 一个 8000 好友的节点就让单次查询走 8000 步。

---

## Prompt 8 — 收尾 review

```
Review recommenders.py against the tests only.
Call out dead code, any rule in the is_valid_recommendations docstring that has
no enforcement, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "If we optimised for recommendation quality rather than matching the baseline
> exactly, I'd switch the score from raw mutual-friend count to **Adamic-Adar**:
> weight each shared friend by `1/log(deg(f))`, so a mutual connection through a
> celebrity with 8k friends counts for far less than one through someone with
> five. That also bounds the 2-hop walk, because I can skip neighbours above a
> degree threshold — their contribution rounds to nothing anyway. Different
> objective, same traversal."
