"""Staged spec for the TTL KV + tag index task. These tests are the contract.

Run against the interview skeleton (project/, starts red):
    python3 -m unittest discover -s tests -v

Run against the reference implementation (solution/, must be green):
    AINC_IMPL=solution python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import os
import sys
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

from clock import FakeClock  # noqa: E402
from store import Store  # noqa: E402

NOW = 1_000.0


def make_store() -> Store:
    return Store(FakeClock(NOW))


# ----------------------------------------------------------------------
# Phase 1 — TTL bugs
# ----------------------------------------------------------------------
class Phase1TTL(unittest.TestCase):
    def setUp(self) -> None:
        self.clock = FakeClock(NOW)
        self.store = Store(self.clock)

    def test_set_get_roundtrip(self) -> None:
        self.store.set("a", 1)
        self.assertEqual(self.store.get("a"), 1)
        self.assertTrue(self.store.exists("a"))
        self.assertEqual(self.store.size(), 1)

    def test_missing_key_is_none(self) -> None:
        self.assertIsNone(self.store.get("missing"))
        self.assertFalse(self.store.exists("missing"))
        self.assertFalse(self.store.delete("missing"))

    def test_overwrite_replaces_value(self) -> None:
        self.store.set("a", 1)
        self.store.set("a", 2)
        self.assertEqual(self.store.get("a"), 2)
        self.assertEqual(self.store.size(), 1)

    def test_delete_removes_key(self) -> None:
        self.store.set("a", 1)
        self.assertTrue(self.store.delete("a"))
        self.assertIsNone(self.store.get("a"))
        self.assertEqual(self.store.size(), 0)

    def test_no_ttl_never_expires(self) -> None:
        self.store.set("a", 1)
        self.clock.advance(10_000)
        self.assertEqual(self.store.get("a"), 1)
        self.assertTrue(self.store.exists("a"))

    def test_exists_and_size_stay_live_before_ttl(self) -> None:
        """ttl is seconds-from-now, not an absolute timestamp.

        Clock starts at 1000. set(..., ttl=10) must still be live at t=1009.
        """
        self.store.set("a", 1, ttl=10)
        self.assertTrue(self.store.exists("a"))
        self.assertEqual(self.store.size(), 1)
        self.clock.advance(9)
        self.assertTrue(self.store.exists("a"))
        self.assertEqual(self.store.size(), 1)

    def test_get_returns_none_once_ttl_elapses(self) -> None:
        self.store.set("a", 1, ttl=10)
        self.clock.advance(10)
        self.assertIsNone(self.store.get("a"))
        self.assertFalse(self.store.exists("a"))
        self.assertEqual(self.store.size(), 0)

    def test_expired_at_the_exact_boundary(self) -> None:
        self.store.set("a", 1, ttl=5)
        self.clock.advance(5)
        self.assertIsNone(self.store.get("a"))

    def test_overwrite_resets_ttl(self) -> None:
        self.store.set("a", 1, ttl=5)
        self.clock.advance(4)
        self.store.set("a", 2, ttl=10)
        self.clock.advance(5)
        self.assertEqual(self.store.get("a"), 2)
        self.clock.advance(5)
        self.assertIsNone(self.store.get("a"))


# ----------------------------------------------------------------------
# Phase 2 — tag query (scan)
# ----------------------------------------------------------------------
class Phase2TagQuery(unittest.TestCase):
    def setUp(self) -> None:
        self.clock = FakeClock(NOW)
        self.store = Store(self.clock)
        self.store.set("red", 1, tags=["color", "warm"])
        self.store.set("blue", 2, tags=["color", "cool"])
        self.store.set("sun", 3, tags=["warm"])
        self.store.set("plain", 4)

    def test_match_all_is_and(self) -> None:
        self.assertEqual(self.store.query(["color", "warm"], match="all"), ["red"])
        self.assertEqual(self.store.query(["color"], match="all"), ["blue", "red"])

    def test_match_any_is_or(self) -> None:
        self.assertEqual(self.store.query(["warm", "cool"], match="any"), ["blue", "red", "sun"])

    def test_empty_tags(self) -> None:
        self.assertEqual(self.store.query([], match="all"), ["blue", "plain", "red", "sun"])
        self.assertEqual(self.store.query([], match="any"), [])

    def test_unknown_tag_is_empty(self) -> None:
        self.assertEqual(self.store.query(["nope"], match="all"), [])
        self.assertEqual(self.store.query(["nope"], match="any"), [])

    def test_query_skips_expired_keys(self) -> None:
        self.store.set("hot", 9, ttl=2, tags=["warm"])
        self.assertEqual(self.store.query(["warm"], match="all"), ["hot", "red", "sun"])
        self.clock.advance(2)
        self.assertEqual(self.store.query(["warm"], match="all"), ["red", "sun"])

    def test_overwrite_replaces_tags(self) -> None:
        self.store.set("red", 1, tags=["cool"])
        self.assertEqual(self.store.query(["warm"], match="all"), ["sun"])
        self.assertEqual(self.store.query(["cool"], match="all"), ["blue", "red"])

    def test_delete_drops_key_from_query(self) -> None:
        self.store.delete("red")
        self.assertEqual(self.store.query(["color"], match="all"), ["blue"])


# ----------------------------------------------------------------------
# Phase 3 — inverted index
# ----------------------------------------------------------------------
class Phase3Scale(unittest.TestCase):
    def test_same_answers_as_the_baseline(self) -> None:
        store = make_store()
        store.set("red", 1, tags=["color", "warm"])
        store.set("blue", 2, tags=["color", "cool"])
        store.set("sun", 3, tags=["warm"])
        store.set("plain", 4)
        self.assertEqual(store.query_fast(["color", "warm"], match="all"), store.query(["color", "warm"], match="all"))
        self.assertEqual(store.query_fast(["warm", "cool"], match="any"), store.query(["warm", "cool"], match="any"))
        self.assertEqual(store.query_fast([], match="all"), store.query([], match="all"))
        self.assertEqual(store.query_fast([], match="any"), store.query([], match="any"))

    def test_overwrite_and_expire_keep_the_index_honest(self) -> None:
        clock = FakeClock(NOW)
        store = Store(clock)
        store.set("a", 1, tags=["hot", "keep"])
        store.set("b", 2, ttl=3, tags=["hot"])
        store.set("a", 3, tags=["cold"])
        self.assertEqual(store.query_fast(["hot"], match="all"), ["b"])
        self.assertEqual(store.query_fast(["cold"], match="all"), ["a"])
        clock.advance(3)
        self.assertEqual(store.query_fast(["hot"], match="all"), [])
        self.assertEqual(store.query_fast(["cold"], match="all"), ["a"])

    def test_repeated_rare_tag_lookups(self) -> None:
        """40k keys, a tag that hits two of them, queried 3000 times.

        Scanning every key on each lookup is ~120M visits and misses the
        budget. An inverted index is two hash lookups.
        """
        store = Store(FakeClock(NOW))
        for i in range(40_000):
            store.set(f"k{i}", i, tags=[f"bucket-{i % 50}", "common"])
        store.set("needle-a", "a", tags=["rare", "common"])
        store.set("needle-b", "b", tags=["rare", "other"])

        start = time.perf_counter()
        for _ in range(3000):
            self.assertEqual(store.query_fast(["rare"], match="all"), ["needle-a", "needle-b"])
        elapsed = time.perf_counter() - start
        self.assertLess(elapsed, 1.0, f"rare-tag loop took {elapsed:.2f}s")

    def test_multi_tag_intersection_is_from_posting_lists(self) -> None:
        store = Store(FakeClock(NOW))
        for i in range(8_000):
            store.set(f"only-a-{i}", i, tags=["a"])
            store.set(f"only-b-{i}", i, tags=["b"])
        store.set("both", 0, tags=["a", "b", "c"])
        start = time.perf_counter()
        self.assertEqual(store.query_fast(["a", "b"], match="all"), ["both"])
        self.assertLess(time.perf_counter() - start, 0.5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
