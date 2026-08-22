# RateLimiter Engine — how to implement this task

案例背景：分布式事件限流与降级系统。你接手已有工程骨架，为 API 网关实现支持**多租户**与**动态时间窗口**的限流引擎，并分阶段完成功能迭代与降级。

```
project/
├── models/request.py          # Request(user_id, endpoint, timestamp, cost)
├── storage/memory_store.py    # KV get/set (swap for Redis later)
├── ratelimiter.py             # main implementation
└── tests/test_ratelimiter.py  # unit tests = the spec
```

This folder is the finished example. The method below is what you do when you only have the skeleton and tests.

---

## 1. Do not write `ratelimiter.py` first

AI-native coding on a skeleton repo is **spec extraction, then staged implementation**.

1. Read `models/request.py` and `storage/memory_store.py` — these constrain the API.
2. Read `tests/test_ratelimiter.py` **before** designing. Test class names are the roadmap:
   - Stage 1 default window
   - Stage 2 tenant + endpoint isolation
   - Stage 3 weighted `cost`
   - Stage 4 sliding window (injected timestamps)
   - Stage 5 hot-reload rules
   - Stage 6 soft degrade + store failure
3. Ask the model for a **requirements list and algorithm choice**, not code. Use [`prompts.md`](./prompts.md).
4. Implement **one stage**, run tests, paste failures, repeat.

If you prompt “implement the whole rate limiter” you get a token bucket, Redis Lua, and thread pools that the tests never asked for.

---

## 2. What the tests actually require

| Constraint | Why it is in the tests |
|------------|------------------------|
| `allow(request) -> Decision` | Gateway needs ALLOW / DEGRADE / DENY, not a boolean |
| Key = `{user_id}:{endpoint}` | Tenant A exhausting `/search` must not block tenant B or `/upload` |
| Quota lives in `MemoryStore` | “Distributed”: process memory is not the source of truth; Redis can replace the store |
| `timestamp` is on the request | Tests inject ms clocks. Never call `time.time()` |
| `cost` is the weight | One request can consume N units |
| Window is sliding | Event at `t=0` expires at `t=window_ms` (strictly older than `now - window`) |
| `set_rule` is hot | Next `allow()` uses the new limit/window |
| Store exception → `DEGRADE` | Gateway stays up; skip origin rather than 500 |

Public API:

```python
limiter = RateLimiter(store, default_limit=10, default_window_ms=1000)
limiter.set_rule("/search", limit=1, window_ms=1000, degrade_threshold=1)
decision = limiter.allow(Request("u1", "/search", timestamp=0, cost=1))
```

Decisions:

- **ALLOW** — `used + cost <= degrade_threshold` (and `<= limit`)
- **DEGRADE** — over the soft threshold but still `<= limit`, **or** the store raised
- **DENY** — `used + cost > limit`. Denied events are **not** recorded

---

## 3. Algorithm: sliding-window log, not token bucket

The skeleton gives a KV store and event timestamps. That maps to a **per-key event log**, not a background refill thread.

```
allow(req):
    rule = rules.get(req.endpoint, default)
    key  = f"{req.user_id}:{req.endpoint}"
    events = store.get(key) or []          # [(ts, cost), ...]
    cutoff = req.timestamp - rule.window_ms
    events = [e for e in events if e.ts > cutoff]
    used = sum(e.cost for e in events)

    if used + req.cost > rule.limit:
        store.set(key, events)             # persist trim even on deny
        return DENY

    events.append((req.timestamp, req.cost))
    store.set(key, events)
    return DEGRADE if used + req.cost > rule.degrade_threshold else ALLOW
```

Why this and not the alternatives:

| Algorithm | Fit for these tests |
|-----------|---------------------|
| Fixed window counter | Fails Stage 4: two requests 1ms apart across a wall-clock boundary would both count as a fresh burst |
| Token bucket | Needs a refill rate and last-refill time. Tests never mention refill; they mention windows and timestamps |
| Sliding window log | Exact, uses `timestamp` + `cost`, works with KV `get/set` |
| Sliding window counter | Approximate; harder to get exact expiry tests green |

Store failure: wrap `get`/`set` in `try/except StoreError` and return `DEGRADE`. Do not crash the request path.

---

## 4. Implement in the same order as the tests

| Stage | Make green | Typical bug if you skip thinking |
|-------|------------|----------------------------------|
| 1 | Default 10/1000ms burst, deny does not consume | Incrementing on deny makes later tests flake |
| 2 | Isolate by `user_id` and `endpoint` | Global counter; or key = user only |
| 3 | `cost` adds N units; `cost=0` does not consume | Treating every request as 1 |
| 4 | Expire with `ts > now - window`; no wall clock | `time.time()`; inclusive boundary keeps t=0 at t=1000 |
| 5 | `set_rule` read on every `allow` | Caching rule into the stored value |
| 6 | Soft band → DEGRADE; `store.fail` → DEGRADE | Letting `StoreError` propagate |

Worked Stage 4 example (`limit=2`, `window_ms=1000`):

```
t=0     ALLOW   log=[0]
t=500   ALLOW   log=[0, 500]
t=999   DENY    still 2 events in window
t=1000  ALLOW   0 is gone (0 > 0 is false), log=[500, 1000]
```

---

## 5. What “distributed” means in this skeleton

There is no Redis in the repo on purpose. The engine is distributed-ready if:

1. All quota is in the store (the limiter holds only **rules**, not counters).
2. `get` then `set` is the atomic unit. In production you replace that with Redis `ZREMRANGEBYSCORE` + `ZADD` + `ZCARD` inside a Lua script so two gateway replicas cannot oversell.
3. When the store is down you **degrade** (fail-open to a cheap path), not fail-closed with 500s.

This example does not implement replica-safe atomicity. That is the natural follow-up after tests are green.

---

## 6. How this demonstrates AI-native practice

You are not using the model as autocomplete for 200 lines. You are using it as:

1. **Spec reader** — dump tests, ask for invariants, no code.
2. **Design reviewer** — sliding log vs token bucket vs fixed window, constrained by `MemoryStore`.
3. **Stage coder** — “implement Stage 1 only; do not touch set_rule.”
4. **Test runner** — you run `unittest`; the model only sees failures you paste.
5. **Failure-mode engineer** — Stage 6 is a product decision (DEGRADE vs DENY), not a syntax fix.

The prompt sequence lives in [`prompts.md`](./prompts.md). After you can reproduce this example, hide `ratelimiter.py` and re-implement from tests only.
