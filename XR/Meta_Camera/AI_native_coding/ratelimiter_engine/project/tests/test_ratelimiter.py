"""Staged unit tests — this file is the spec. Implement ratelimiter.py to make it green.

Run from project/:
    python3 -m unittest tests.test_ratelimiter -v
"""

from __future__ import annotations

import sys
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from models.request import Request
from ratelimiter import Decision, RateLimiter
from storage.memory_store import MemoryStore


def _req(
    user: str = "u1",
    endpoint: str = "/api",
    ts: int = 0,
    cost: int = 1,
) -> Request:
    return Request(user_id=user, endpoint=endpoint, timestamp=ts, cost=cost)


class Stage1DefaultWindow(unittest.TestCase):
    """Fixed default rule: 10 units / 1000ms, no soft-degrade band."""

    def setUp(self) -> None:
        self.limiter = RateLimiter(MemoryStore())

    def test_first_request_allowed(self) -> None:
        self.assertEqual(self.limiter.allow(_req(ts=0)), Decision.ALLOW)

    def test_burst_up_to_limit_then_deny(self) -> None:
        for i in range(10):
            self.assertEqual(self.limiter.allow(_req(ts=i)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(ts=10)), Decision.DENY)

    def test_denied_request_does_not_consume_quota(self) -> None:
        for i in range(10):
            self.limiter.allow(_req(ts=0))
        self.assertEqual(self.limiter.allow(_req(ts=0)), Decision.DENY)
        self.assertEqual(self.limiter.allow(_req(ts=0)), Decision.DENY)


class Stage2MultiTenantAndEndpoint(unittest.TestCase):
    def setUp(self) -> None:
        self.limiter = RateLimiter(MemoryStore(), default_limit=2, default_window_ms=1000)

    def test_tenants_are_isolated(self) -> None:
        self.assertEqual(self.limiter.allow(_req(user="a", ts=0)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(user="a", ts=1)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(user="a", ts=2)), Decision.DENY)
        self.assertEqual(self.limiter.allow(_req(user="b", ts=2)), Decision.ALLOW)

    def test_endpoints_are_isolated(self) -> None:
        self.assertEqual(self.limiter.allow(_req(endpoint="/search", ts=0)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(endpoint="/search", ts=1)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(endpoint="/search", ts=2)), Decision.DENY)
        self.assertEqual(self.limiter.allow(_req(endpoint="/upload", ts=2)), Decision.ALLOW)


class Stage3CostWeighted(unittest.TestCase):
    def setUp(self) -> None:
        self.limiter = RateLimiter(MemoryStore(), default_limit=5, default_window_ms=1000)

    def test_cost_consumes_multiple_units(self) -> None:
        self.assertEqual(self.limiter.allow(_req(ts=0, cost=3)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(ts=1, cost=2)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(ts=2, cost=1)), Decision.DENY)

    def test_single_request_heavier_than_limit_is_denied(self) -> None:
        self.assertEqual(self.limiter.allow(_req(ts=0, cost=6)), Decision.DENY)

    def test_zero_cost_does_not_consume(self) -> None:
        for _ in range(5):
            self.limiter.allow(_req(ts=0, cost=1))
        self.assertEqual(self.limiter.allow(_req(ts=0, cost=0)), Decision.ALLOW)


class Stage4SlidingWindow(unittest.TestCase):
    def setUp(self) -> None:
        self.limiter = RateLimiter(MemoryStore(), default_limit=2, default_window_ms=1000)

    def test_oldest_event_expires_exactly_at_window_boundary(self) -> None:
        self.assertEqual(self.limiter.allow(_req(ts=0)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(ts=500)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(ts=999)), Decision.DENY)
        # event at t=0 is gone once now - window_ms == 0 (strictly older)
        self.assertEqual(self.limiter.allow(_req(ts=1000)), Decision.ALLOW)

    def test_does_not_use_wall_clock(self) -> None:
        self.limiter.allow(_req(ts=10_000))
        self.limiter.allow(_req(ts=10_001))
        self.assertEqual(self.limiter.allow(_req(ts=10_002)), Decision.DENY)


class Stage5DynamicRules(unittest.TestCase):
    def setUp(self) -> None:
        self.limiter = RateLimiter(MemoryStore(), default_limit=100, default_window_ms=1000)

    def test_per_endpoint_rule_overrides_default(self) -> None:
        self.limiter.set_rule("/search", limit=1, window_ms=1000)
        self.assertEqual(self.limiter.allow(_req(endpoint="/search", ts=0)), Decision.ALLOW)
        self.assertEqual(self.limiter.allow(_req(endpoint="/search", ts=1)), Decision.DENY)
        self.assertEqual(self.limiter.allow(_req(endpoint="/other", ts=1)), Decision.ALLOW)

    def test_hot_reload_tighter_limit(self) -> None:
        self.limiter.set_rule("/api", limit=3, window_ms=1000)
        self.limiter.allow(_req(ts=0))
        self.limiter.allow(_req(ts=1))
        self.limiter.set_rule("/api", limit=2, window_ms=1000)
        self.assertEqual(self.limiter.allow(_req(ts=2)), Decision.DENY)

    def test_hot_reload_shorter_window_drops_old_events(self) -> None:
        self.limiter.set_rule("/api", limit=1, window_ms=5000)
        self.assertEqual(self.limiter.allow(_req(ts=0)), Decision.ALLOW)
        self.limiter.set_rule("/api", limit=1, window_ms=100)
        # t=0 is outside the new 100ms window at t=200
        self.assertEqual(self.limiter.allow(_req(ts=200)), Decision.ALLOW)


class Stage6Degrade(unittest.TestCase):
    def test_soft_limit_returns_degrade_and_still_consumes(self) -> None:
        limiter = RateLimiter(MemoryStore(), default_limit=5, default_window_ms=1000)
        limiter.set_rule("/api", limit=5, window_ms=1000, degrade_threshold=3)
        self.assertEqual(limiter.allow(_req(ts=0, cost=3)), Decision.ALLOW)
        self.assertEqual(limiter.allow(_req(ts=1, cost=1)), Decision.DEGRADE)
        self.assertEqual(limiter.allow(_req(ts=2, cost=1)), Decision.DEGRADE)
        self.assertEqual(limiter.allow(_req(ts=3, cost=1)), Decision.DENY)

    def test_store_failure_degrades_instead_of_raising(self) -> None:
        store = MemoryStore()
        limiter = RateLimiter(store, default_limit=1, default_window_ms=1000)
        self.assertEqual(limiter.allow(_req(ts=0)), Decision.ALLOW)
        store.fail = True
        self.assertEqual(limiter.allow(_req(ts=1)), Decision.DEGRADE)

    def test_store_failure_on_first_request_still_degrades(self) -> None:
        store = MemoryStore()
        store.fail = True
        limiter = RateLimiter(store)
        self.assertEqual(limiter.allow(_req(ts=0)), Decision.DEGRADE)


class Stage7ThreadSafety(unittest.TestCase):
    def test_same_tenant_never_oversells_under_contention(self) -> None:
        limiter = RateLimiter(MemoryStore(), default_limit=50, default_window_ms=10_000)
        n_threads = 16
        per_thread = 20
        start = threading.Barrier(n_threads)
        accepted = [0]
        lock = threading.Lock()

        def worker() -> None:
            start.wait()
            local = 0
            for i in range(per_thread):
                if limiter.allow(_req(user="acme", ts=i)) != Decision.DENY:
                    local += 1
            with lock:
                accepted[0] += local

        threads = [threading.Thread(target=worker) for _ in range(n_threads)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(accepted[0], 50)

    def test_tenants_keep_independent_quota_under_contention(self) -> None:
        limiter = RateLimiter(MemoryStore(), default_limit=10, default_window_ms=10_000)
        tenants = ("acme", "globex")
        n_threads = 8
        per_thread = 10
        start = threading.Barrier(len(tenants) * n_threads)
        accepted = {t: 0 for t in tenants}
        lock = threading.Lock()

        def worker(tenant: str) -> None:
            start.wait()
            local = 0
            for i in range(per_thread):
                if limiter.allow(_req(user=tenant, ts=i)) != Decision.DENY:
                    local += 1
            with lock:
                accepted[tenant] += local

        threads = [
            threading.Thread(target=worker, args=(tenant,))
            for tenant in tenants
            for _ in range(n_threads)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(accepted["acme"], 10)
        self.assertEqual(accepted["globex"], 10)


if __name__ == "__main__":
    unittest.main()
