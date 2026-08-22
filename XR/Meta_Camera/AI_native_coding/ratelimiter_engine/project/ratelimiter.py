from __future__ import annotations

import threading
from collections import Counter
from contextlib import contextmanager
from dataclasses import dataclass
from enum import Enum
from typing import Iterator, Optional

from models.request import Request
from storage.memory_store import MemoryStore, StoreError


class Decision(str, Enum):
    """Gateway action for one request.

    ALLOW   — forward to origin
    DEGRADE — do not hit origin; serve fallback / cached / cheaper path
    DENY    — reject (HTTP 429)
    """

    ALLOW = "allow"
    DEGRADE = "degrade"
    DENY = "deny"


@dataclass(frozen=True)
class Rule:
    limit: int
    window_ms: int
    degrade_threshold: int


class _KeyedLocks:
    """One mutex per quota key so tenant A does not block tenant B."""

    def __init__(self) -> None:
        self._guard = threading.Lock()
        self._locks: dict[str, threading.Lock] = {}

    @contextmanager
    def hold(self, key: str) -> Iterator[None]:
        with self._guard:
            lock = self._locks.get(key)
            if lock is None:
                lock = threading.Lock()
                self._locks[key] = lock
        with lock:
            yield


class RateLimiter:
    """Sliding-window, cost-weighted limiter. Authoritative state lives in `store`.

    Key: `{user_id}:{endpoint}` so tenants and routes are isolated.
    Value: list of (timestamp_ms, cost) events still relevant to the current window.

    Thread safety: set_rule vs allow is serialized on `_rules_lock`.
    The get-modify-set of a quota key is serialized on a per-key lock so two
    workers for the same tenant cannot oversell, while other tenants proceed.
    """

    def __init__(
        self,
        store: MemoryStore,
        *,
        default_limit: int = 10,
        default_window_ms: int = 1000,
        default_degrade_threshold: Optional[int] = None,
    ) -> None:
        if default_limit < 0 or default_window_ms <= 0:
            raise ValueError("default_limit must be >= 0 and default_window_ms > 0")
        degrade = (
            default_limit
            if default_degrade_threshold is None
            else default_degrade_threshold
        )
        self._store = store
        self._default = Rule(default_limit, default_window_ms, degrade)
        self._rules: dict[str, Rule] = {}
        self._rules_lock = threading.Lock()
        self._key_locks = _KeyedLocks()

    def set_rule(
        self,
        endpoint: str,
        *,
        limit: int,
        window_ms: int,
        degrade_threshold: Optional[int] = None,
    ) -> None:
        """Hot-reload per-endpoint quota. Next allow() uses the new window/limit."""
        if limit < 0 or window_ms <= 0:
            raise ValueError("limit must be >= 0 and window_ms > 0")
        if degrade_threshold is None:
            degrade_threshold = limit
        if degrade_threshold < 0 or degrade_threshold > limit:
            raise ValueError("degrade_threshold must be in [0, limit]")
        with self._rules_lock:
            self._rules[endpoint] = Rule(limit, window_ms, degrade_threshold)

    def _rule_for(self, endpoint: str) -> Rule:
        with self._rules_lock:
            return self._rules.get(endpoint, self._default)

    def allow(self, request: Request) -> Decision:
        if request.cost < 0:
            raise ValueError("cost must be >= 0")

        rule = self._rule_for(request.endpoint)
        key = f"{request.user_id}:{request.endpoint}"

        with self._key_locks.hold(key):
            return self._allow_locked(key, rule, request)

    def _allow_locked(self, key: str, rule: Rule, request: Request) -> Decision:
        try:
            events = list(self._store.get(key) or [])
        except StoreError:
            return Decision.DEGRADE

        cutoff = request.timestamp - rule.window_ms
        events = [e for e in events if e[0] > cutoff]
        used = sum(cost for _, cost in events)
        projected = used + request.cost

        if projected > rule.limit:
            try:
                self._store.set(key, events)
            except StoreError:
                return Decision.DEGRADE
            return Decision.DENY

        events.append((request.timestamp, request.cost))
        try:
            self._store.set(key, events)
        except StoreError:
            return Decision.DEGRADE

        if projected > rule.degrade_threshold:
            return Decision.DEGRADE
        return Decision.ALLOW


def main() -> None:
    """Drive the limiter from many tenant worker threads and check no oversell."""
    store = MemoryStore()
    limiter = RateLimiter(store, default_limit=10, default_window_ms=1000)
    limiter.set_rule("/search", limit=20, window_ms=10_000, degrade_threshold=16)
    limiter.set_rule("/upload", limit=8, window_ms=10_000, degrade_threshold=6)

    tenants = ("acme", "globex", "initech")
    workers_per_tenant = 4
    requests_per_worker = 15
    endpoint = "/search"
    limit = 20

    n_workers = len(tenants) * workers_per_tenant
    start = threading.Barrier(n_workers)
    tallies: dict[str, Counter[Decision]] = {t: Counter() for t in tenants}
    tally_lock = threading.Lock()

    def worker(tenant: str) -> None:
        start.wait()
        local: Counter[Decision] = Counter()
        for i in range(requests_per_worker):
            decision = limiter.allow(
                Request(
                    user_id=tenant,
                    endpoint=endpoint,
                    timestamp=i,
                    cost=1,
                )
            )
            local[decision] += 1
        with tally_lock:
            tallies[tenant].update(local)

    threads = [
        threading.Thread(target=worker, args=(tenant,), name=f"{tenant}-{i}")
        for tenant in tenants
        for i in range(workers_per_tenant)
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    print("Multi-tenant RateLimiter (thread-safe)")
    print(
        f"  tenants={','.join(tenants)}  workers/tenant={workers_per_tenant}  "
        f"requests/worker={requests_per_worker}  endpoint={endpoint}"
    )
    print(f"  rule: limit={limit}  degrade_threshold=16  window_ms=10000")
    print()
    print(f"  {'tenant':<10} {'allow':>7} {'degrade':>8} {'deny':>7} {'accepted':>9} {'limit':>6}")

    ok = True
    for tenant in tenants:
        allow = tallies[tenant][Decision.ALLOW]
        degrade = tallies[tenant][Decision.DEGRADE]
        deny = tallies[tenant][Decision.DENY]
        accepted = allow + degrade
        if accepted != limit:
            ok = False
        print(
            f"  {tenant:<10} {allow:>7} {degrade:>8} {deny:>7} {accepted:>9} {limit:>6}"
        )

    print()
    if ok:
        print("Thread-safety check: each tenant accepted exactly its limit; no oversell.")
    else:
        raise SystemExit("Thread-safety check FAILED: a tenant oversold or undersold quota.")


if __name__ == "__main__":
    main()
