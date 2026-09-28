# Prompts to use on this skeleton

Paste these in order. Do not skip to a “write the whole file” prompt until Stage 1–4 are green.

Replace nothing except file contents the agent already has in context.

---

## Prompt 0 — extract the spec (no code)

```
Read models/request.py, storage/memory_store.py, and tests/test_ratelimiter.py.

List:
1. The public API I must implement (signatures and return types)
2. Invariants implied by each test class
3. Edge cases (deny consumption, zero cost, store failure, window boundary)
4. Anything the tests do NOT require (so we will not build it)

Do not write implementation code.
```

---

## Prompt 1 — pick the algorithm (no code)

```
The store is a KV get/set. Requests carry timestamp (ms) and cost.
Tests require a sliding window, per (user_id, endpoint) isolation,
hot-reloadable rules, and DEGRADE when the store raises.

Compare fixed window, token bucket, and sliding-window log.
Recommend one algorithm and the exact key/value layout in MemoryStore.
Do not write ratelimiter.py yet.
```

---

## Prompt 2 — Stage 1 only

```
Implement RateLimiter.allow() so Stage1DefaultWindow passes.
Use default_limit=10, default_window_ms=1000.
Persist the event log in MemoryStore. Do not add set_rule, cost weighting,
or degrade yet unless the existing tests already need them.
Then I will run: python3 -m unittest tests.test_ratelimiter.Stage1DefaultWindow -v
```

If it fails, paste the unittest output and:

```
Fix only the failure above. Do not refactor unrelated stages.
```

---

## Prompt 3 — Stages 2–3

```
Stage1 is green. Extend keying so user_id and endpoint are isolated,
and so request.cost is the number of units consumed (cost=0 consumes nothing).
Do not change the Decision type or start using time.time().
```

---

## Prompt 4 — Stage 4 window math

```
Expire events with timestamp <= now - window_ms (i.e. keep timestamp > cutoff).
Use request.timestamp only. A request at t=0 must not still count at t=window_ms.
Denied requests must not be appended to the log.
```

---

## Prompt 5 — Stage 5 dynamic rules

```
Add set_rule(endpoint, limit, window_ms, degrade_threshold=None).
allow() must read the latest rule every call. Do not bake the window into the stored value.
When the window shrinks, old events outside the new window must drop.
```

---

## Prompt 6 — Stage 6 degrade

```
Add Decision.DEGRADE:
- if used+cost > degrade_threshold and <= limit, record the event and return DEGRADE
- if MemoryStore.get/set raises StoreError, return DEGRADE and do not crash
Denied events still must not be recorded.
Run the full file: python3 -m unittest tests.test_ratelimiter -v
```

---

## Prompt 7 — review (after green)

```
Review ratelimiter.py against the tests only.
Call out: read-modify-write races if two gateways share the store,
what happens if set_rule grows the window after we already trimmed,
and whether we should fail-open or fail-closed. Do not add code unless I ask.
```

That last prompt is the AI-native close: the model audits, you decide.
