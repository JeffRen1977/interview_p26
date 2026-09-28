# TTL KV Index (C++) — 逐阶段 Prompt

按顺序用。每个 Phase 结束先跑测试再进下一个。

Python 对照：[`../ttl_kv_index/prompts.md`](../ttl_kv_index/prompts.md)。C++ 里 `delete` 叫 `erase`。

---

## Prompt 0 — 抽 spec（不写代码）

```
Read clock.h, store.h, store.cpp and tests/test_ttl_kv_index.cpp.
Do NOT write implementation code.

List:
1. The exact contract of set/get/exists/size with TTL, including when a key
   expires relative to clock.now()
2. Whether ttl is a duration from now or an absolute timestamp
3. query / query_fast: match="all" vs "any", and what empty tags mean for each
4. What the Phase 3 stress shapes are and what each one is punishing
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Four Phase-1 failures:
  test_exists_and_size_stay_live_before_ttl
  test_get_returns_none_once_ttl_elapses
  test_expired_at_the_exact_boundary
  test_overwrite_resets_ttl

Explain each root cause in one sentence, naming the exact line in store.cpp.
The test clock starts at t=1000 — include that fact in the TTL explanation.
Do NOT propose code yet.
```

---

## Prompt 2 — Phase 1 修复

```
Task: fix set and get in store.cpp so TTL is honoured.
Constraints:
- ttl is seconds from clock.now(); nullopt means never expire
- expires_at = nullopt if !ttl else clock.now() + *ttl
- A key is expired iff now >= expires_at (inclusive)
- get of an expired key returns nullopt; lazy eviction (erasing the entry) is OK
- overwrite replaces value, ttl and tags
- Signatures unchanged; do not implement query or query_fast
Return only the changed methods.
```

跑：`make test`（先看 Phase1 四条是否变绿）

---

## Prompt 3 — Phase 2 扫描查询

```
Task: implement Store::query(tags, match="all") in store.cpp.
Constraints:
- match="all": key must contain every requested tag; empty tags → every live key
- match="any": key must contain at least one requested tag; empty tags → {}
- Skip expired keys; return sorted keys
- Scanning all entries is fine
- Overwrite replaces the whole tag set
- Do NOT touch query_fast
Return only the function body.
```

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste test output>
```

---

## Prompt 5 — Phase 3（先说瓶颈，再让它写）

```
The stress loop is 3000 lookups of a rare tag against 40k keys. Scanning every
key each time is ~120M visits and misses the budget. Tags are a low-cardinality
side index.

Task: implement query_fast as an inverted index tag -> unordered_set(keys).
Constraints:
- Maintain the index in set / erase / lazy-expire; overwrite must drop old tags
  before adding new ones, otherwise query_fast returns ghosts
- match="all" = posting-list intersection; match="any" = union
- After selecting candidates, lazy-expire them so dead keys never appear
- Must return exactly the same list as query; keep query as the scan so I can
  differential-test them
- Do not add dependencies
Return the index helpers plus query_fast.
```

跑：`make test IMPL=project` 全绿即过。对照答案：`make test IMPL=solution`

---

## Prompt 6 — 复杂度确认

```
State the complexity of query_fast in terms of #tags (T), posting-list size of
the rarest tag (P), and #keys (N). Tell me when the inverted index is worse
than a scan.
Do NOT change code.
```

（期望答案：`all` 是 O(T·P_min) 量级的求交，稀有 tag 时 P≪N；当每个 tag 的选择度都接近 1、P≈N 时倒排退化为扫表还多付写放大。）

---

## Prompt 7 — 收尾 review

```
Review store.cpp against the tests only.
Call out dead code, index leaks on expiry, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句：

> "I considered a heap of expiry times for eager purge. I didn't add it: the
> stress set is query-bound, not expiry-bound, and lazy eviction on read/query
> is enough for the contract. A min-heap would pay off if we had a background
> sweeper or a size() that must be O(1) after a large batch of expirations —
> different bottleneck."
