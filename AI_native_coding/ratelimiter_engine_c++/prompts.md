# RateLimiter Engine (C++) — 逐阶段 Prompt

> 这道题最容易被 AI 带跑偏。**开局绝不能说"帮我实现一个限流器"** ——
> 你会拿到令牌桶 + Redis Lua + 线程池，全是测试没要求的东西。

---

## Prompt 0 — 抽 spec（不写代码，最重要的一条）

```
Read request.h, memory_store.h, ratelimiter.h and tests/test_ratelimiter.cpp.
Do NOT write implementation code.

List:
1. The public API I must implement, with exact signatures
2. What each Stage class in the test file is asserting, one line each
3. The exact sliding-window boundary rule: is an event at t still counted at
   t + window_ms?
4. Whether a denied request consumes quota, and whether a degraded one does
5. Everything the tests do NOT require, so we don't build it
```

第 5 条是精髓。期望它明确说出：**不需要**令牌桶、不需要 Redis、不需要漏桶、不需要分布式共识、不需要后台清理线程。

---

## Prompt 1 — Stage 1–2（默认窗口 + 隔离）

```
Task: implement RateLimiter::allow for Stage 1 and Stage 2 only.
Constraints:
- Quota key is "{user_id}:{endpoint}" -- tenants and endpoints are isolated
- Authoritative state lives in MemoryStore, never in RateLimiter's own fields
- Return Decision::Allow until used + cost > limit, then Decision::Deny
- A DENIED request must consume nothing
- Do NOT implement cost weighting, degrade, hot reload or locking yet
- Do NOT add a token bucket, a leaky bucket, Redis, or a background thread
Return only the function bodies.
```

跑：`make test`（只看 Stage1/Stage2 那两段）

---

## Prompt 2 — Stage 3–4（权重 + 滑动窗口）

```
Task: extend allow() with cost weighting and a sliding window.
Constraints:
- `used` is the SUM of costs of live events, not their count
- An event at timestamp t is live iff t > now - window_ms (STRICTLY greater).
  At exactly now - window_ms it has expired. The test asserts that t=0 is gone
  at t=1000 with a 1000ms window
- Timestamps come from request.timestamp. NEVER call any clock -- not
  std::chrono, not time(), not steady_clock
- cost == 0 must always be allowed if it does not push past the limit
- cost < 0 throws std::invalid_argument
Return only the changed function.
```

跑：`make test`

---

## Prompt 3 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste the failing test name and its message>
```

---

## Prompt 4 — Stage 5（热更新，把陷阱写进 prompt）

```
Task: add set_rule() and per-endpoint rule lookup.
Constraints:
- set_rule(endpoint, limit, window_ms, degrade_threshold) hot-reloads: the very
  next allow() uses the new values
- degrade_threshold defaults to `limit` and must be in [0, limit]; limit >= 0
  and window_ms > 0, otherwise std::invalid_argument
- Snapshot the Rule ONCE per allow(), BY VALUE, before taking any per-key lock.
  Re-reading it mid-decision would let a concurrent set_rule split one decision
  across two rules -- pruning with the old window and comparing to the new limit
- Window pruning happens at READ time, in allow(), using the current rule.
  Do NOT prune at write time: that would make a shortened window take no effect
  until new traffic arrives, and the test checks the opposite
Return only the changed functions.
```

跑：`make test`

---

## Prompt 5 — Stage 6（降级）

```
Task: add the two degrade paths.
Constraints:
- Soft band: used + cost > degrade_threshold but <= limit -> Decision::Degrade,
  and the request STILL consumes quota
- Store failure: any StoreError from get() or set() -> Decision::Degrade, and
  the request consumes NOTHING (a later test flips the store back on and
  expects the quota to be intact)
- Catch StoreError specifically, not `...` -- swallowing every exception would
  hide std::invalid_argument from a bad cost
- Do not retry, do not add a circuit breaker, do not log
Return only the changed function.
```

跑：`make test`

---

## Prompt 6 — Stage 7（线程安全，C++ 版的真考点）

```
Task: make RateLimiter safe for concurrent allow() and set_rule().
Constraints:
- The read-modify-write of ONE quota key must be atomic: read window, prune,
  decide, write back
- Use one mutex PER KEY, not a global one: a global lock would serialise
  tenants, which defeats the isolation requirement the whole design is built on
- KeyedLocks holds std::unordered_map<std::string, std::mutex>:
    * std::mutex is neither copyable nor movable, so construct in place with
      try_emplace -- insert({key, std::mutex{}}) will not compile
    * unordered_map is node-based, so a reference to a mapped value stays valid
      across a rehash; that is what makes returning std::mutex& safe. Say so in
      a comment
    * hold the map's own guard mutex ONLY while looking the key up; release it
      before locking the per-key mutex, or every tenant serialises again
- Guard the rules map with its own mutex; rule_for returns a COPY
Return only the changed code.
```

跑：

```bash
make test
make clean
make test CXXFLAGS="-std=c++17 -g -O1 -Wall -Wextra -pthread -fsanitize=thread"
```

**面试里主动跑第二条。** ThreadSanitizer 干净地过一遍多线程测试，比任何"我注意了线程安全"的口头说明都有说服力。

---

## Prompt 7 — 锁设计审查（只问，不写）

```
Review the locking in ratelimiter.cpp. Answer these, do NOT change code:
1. Which two mutexes exist and what invariant does each protect?
2. Is there any path that holds both at once? If so, is the order consistent?
3. What would break if KeyedLocks used std::vector<std::mutex> instead of
   unordered_map?
4. What would break if rule_for returned a const Rule& instead of a copy?
```

期望答案：(2) 没有嵌套持有 —— `guard_` 在返回前释放；(3) `vector` 扩容会移动元素，而 `std::mutex` 不可移动，且引用会失效；(4) 返回引用后，并发的 `set_rule` 会在决策中途改掉规则内容，形成撕裂读。

---

## Prompt 8 — 收尾 review

```
Review ratelimiter.cpp against the tests only.
Call out: any clock call, any catch(...) that is too broad, any state kept in
RateLimiter's own fields that should live in the store, any lock held longer
than necessary, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说两句（面试官在等这两句）：

> "The fail-open choice — degrading instead of denying when the store is down —
> is a product decision, not a technical one. Losing the gateway usually costs
> more than briefly overshooting a quota. But it isn't universal: if this
> limiter guarded paid quota or abuse prevention, fail-closed would be right.
> I'd want that written down next to the constant."

> "And the moment this store becomes Redis, the per-key mutex stops being
> sufficient — the get-modify-set is no longer atomic across processes. The two
> real options are pushing the whole decision into a Lua script, or switching to
> INCR plus EXPIRE and accepting a fixed-window approximation. The current
> design keeps that door open because all authoritative state already lives in
> the store, not in the limiter."
