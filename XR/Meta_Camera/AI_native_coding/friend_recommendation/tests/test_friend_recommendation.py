"""Staged spec for the friend-recommendation task. These tests are the contract.

Run against the interview skeleton (project/, starts red):
    python3 -m unittest discover -s tests -v

Run against the reference implementation (solution/, must be green):
    AINC_IMPL=solution python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import os
import random
import sys
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

from recommenders import (  # noqa: E402
    evaluate,
    is_valid_recommendations,
    recommend_baseline,
    recommend_fast,
)
from social_graph import SocialGraph  # noqa: E402


def tiny_graph() -> SocialGraph:
    """alice-bob-carol triangle-ish cluster plus a far-away pair.

        alice - bob
        alice - carol
        bob   - dave
        carol - dave
        dave  - erin
        frank - grace   (disconnected component)

    alice's friends = {bob, carol}; dave shares both of them.
    """
    graph = SocialGraph()
    for a, b in [
        ("alice", "bob"),
        ("alice", "carol"),
        ("bob", "dave"),
        ("carol", "dave"),
        ("dave", "erin"),
        ("frank", "grace"),
    ]:
        graph.add_friendship(a, b)
    return graph


def random_graph(seed: int, n_users: int, edges_per_user: int) -> SocialGraph:
    rng = random.Random(seed)
    ids = [f"u{i:05d}" for i in range(n_users)]
    graph = SocialGraph(ids)
    for uid in ids:
        for _ in range(edges_per_user):
            other = rng.choice(ids)
            if other != uid:
                graph.add_friendship(uid, other)
    return graph


# ----------------------------------------------------------------------
# Phase 1 — validation rules
# ----------------------------------------------------------------------
class Phase1Validation(unittest.TestCase):
    def setUp(self) -> None:
        self.graph = tiny_graph()

    def test_accepts_clean_candidate_list(self) -> None:
        self.assertTrue(is_valid_recommendations(self.graph, "alice", ["dave", "erin"]))

    def test_empty_candidate_list_is_valid(self) -> None:
        self.assertTrue(is_valid_recommendations(self.graph, "alice", []))

    def test_rejects_self_recommendation(self) -> None:
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["dave", "alice"]))

    def test_rejects_duplicates(self) -> None:
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["dave", "dave"]))

    def test_rejects_unknown_source_user(self) -> None:
        self.assertFalse(is_valid_recommendations(self.graph, "nobody", []))

    def test_rejects_existing_friend(self) -> None:
        """bob is already alice's friend, so he is not a recommendation."""
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["bob"]))
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["dave", "carol"]))

    def test_rejects_unknown_candidate(self) -> None:
        """Ids that are not in the graph cannot be recommended."""
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["ghost"]))
        self.assertFalse(is_valid_recommendations(self.graph, "alice", ["dave", "ghost"]))


# ----------------------------------------------------------------------
# Phase 2 — baseline ranking
# ----------------------------------------------------------------------
class Phase2Baseline(unittest.TestCase):
    def setUp(self) -> None:
        self.graph = tiny_graph()

    def test_ranks_by_mutual_friend_count(self) -> None:
        # dave shares bob and carol with alice (2); erin shares nobody.
        self.assertEqual(recommend_baseline(self.graph, "alice", k=5), ["dave"])

    def test_excludes_self_and_existing_friends(self) -> None:
        recs = recommend_baseline(self.graph, "dave", k=5)
        self.assertNotIn("dave", recs)
        self.assertNotIn("bob", recs)
        self.assertNotIn("carol", recs)
        self.assertNotIn("erin", recs)
        self.assertEqual(recs, ["alice"])

    def test_ties_break_by_user_id(self) -> None:
        graph = SocialGraph()
        graph.add_friendship("me", "hub")
        for name in ["zoe", "adam", "mia"]:
            graph.add_friendship("hub", name)
        self.assertEqual(recommend_baseline(graph, "me", k=3), ["adam", "mia", "zoe"])

    def test_respects_k(self) -> None:
        graph = SocialGraph()
        graph.add_friendship("me", "hub")
        for name in ["zoe", "adam", "mia"]:
            graph.add_friendship("hub", name)
        self.assertEqual(recommend_baseline(graph, "me", k=2), ["adam", "mia"])
        self.assertEqual(recommend_baseline(graph, "me", k=0), [])

    def test_unknown_user_returns_empty(self) -> None:
        self.assertEqual(recommend_baseline(self.graph, "nobody", k=5), [])

    def test_no_mutual_friends_returns_empty(self) -> None:
        self.graph.add_user("loner")
        self.assertEqual(recommend_baseline(self.graph, "loner", k=5), [])
        self.assertEqual(recommend_baseline(self.graph, "frank", k=5), [])

    def test_output_passes_the_phase1_validator(self) -> None:
        for user in self.graph.users():
            recs = recommend_baseline(self.graph, user, k=5)
            self.assertTrue(
                is_valid_recommendations(self.graph, user, recs),
                f"{user} -> {recs} is not a legal recommendation list",
            )


# ----------------------------------------------------------------------
# Phase 3 — offline evaluation
# ----------------------------------------------------------------------
class Phase3Evaluate(unittest.TestCase):
    def setUp(self) -> None:
        self.graph = tiny_graph()

    def test_perfect_recommender_scores_one(self) -> None:
        holdout = {"alice": {"dave"}}
        scores = evaluate(self.graph, lambda g, u, k: ["dave"], holdout, k=1)
        self.assertAlmostEqual(scores["precision"], 1.0)
        self.assertAlmostEqual(scores["recall"], 1.0)
        self.assertAlmostEqual(scores["coverage"], 1.0)

    def test_precision_divides_by_k_not_by_len_recs(self) -> None:
        """One hit out of k=4 slots is 0.25, even though only one id came back."""
        holdout = {"alice": {"dave"}}
        scores = evaluate(self.graph, lambda g, u, k: ["dave"], holdout, k=4)
        self.assertAlmostEqual(scores["precision"], 0.25)
        self.assertAlmostEqual(scores["recall"], 1.0)

    def test_recall_uses_holdout_size(self) -> None:
        holdout = {"alice": {"dave", "erin"}}
        scores = evaluate(self.graph, lambda g, u, k: ["dave", "grace"], holdout, k=2)
        self.assertAlmostEqual(scores["precision"], 0.5)
        self.assertAlmostEqual(scores["recall"], 0.5)

    def test_metrics_average_over_users(self) -> None:
        holdout = {"alice": {"dave"}, "frank": {"erin"}}
        scores = evaluate(self.graph, recommend_baseline, holdout, k=1)
        # alice -> ["dave"] hits; frank has no 2-hop neighbour so it returns [].
        self.assertAlmostEqual(scores["precision"], 0.5)
        self.assertAlmostEqual(scores["recall"], 0.5)
        self.assertAlmostEqual(scores["coverage"], 0.5)

    def test_empty_holdout_is_zero(self) -> None:
        scores = evaluate(self.graph, recommend_baseline, {}, k=3)
        self.assertEqual(scores, {"precision": 0.0, "recall": 0.0, "coverage": 0.0})

    def test_non_positive_k_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            evaluate(self.graph, recommend_baseline, {"alice": {"dave"}}, k=0)


# ----------------------------------------------------------------------
# Phase 4 — scale
# ----------------------------------------------------------------------
class Phase4Scale(unittest.TestCase):
    def test_same_answers_as_the_baseline(self) -> None:
        graph = random_graph(seed=1234, n_users=400, edges_per_user=3)
        for user in graph.users()[:120]:
            self.assertEqual(
                recommend_fast(graph, user, k=5),
                recommend_baseline(graph, user, k=5),
                f"fast and baseline disagree for {user}",
            )
        self.assertEqual(recommend_fast(graph, "nobody", k=5), [])
        self.assertEqual(recommend_fast(graph, graph.users()[0], k=0), [])

    def test_fast_output_passes_the_validator(self) -> None:
        graph = random_graph(seed=99, n_users=200, edges_per_user=3)
        for user in graph.users():
            recs = recommend_fast(graph, user, k=5)
            self.assertTrue(is_valid_recommendations(graph, user, recs))

    def test_large_sparse_graph_many_queries(self) -> None:
        """40k users, ~4 friends each, 2000 lookups.

        Scoring every user on every call is 2000 x 40k set intersections. Only
        friends-of-friends can score at all, and there are ~16 of those.
        """
        graph = random_graph(seed=7, n_users=40_000, edges_per_user=2)
        probes = graph.users()[:2000]
        start = time.perf_counter()
        for user in probes:
            recommend_fast(graph, user, k=5)
        elapsed = time.perf_counter() - start
        self.assertLess(elapsed, 1.5, f"2000 lookups took {elapsed:.2f}s")

    def test_high_degree_hub_is_handled(self) -> None:
        """One celebrity with 8k friends: every fan shares exactly that one hop.

        Walking the hub's adjacency is unavoidable if the answer has to match
        the baseline, so this pins correctness on the shape, not speed. Be ready
        to say out loud when 2-hop expansion is the *wrong* trade (see README).
        """
        graph = SocialGraph()
        for i in range(8_000):
            graph.add_friendship("celebrity", f"fan{i:05d}")
        recs = recommend_fast(graph, "fan00000", k=3)
        self.assertEqual(recs, ["fan00001", "fan00002", "fan00003"])
        self.assertEqual(recs, recommend_baseline(graph, "fan00000", k=3))
        self.assertEqual(recommend_fast(graph, "celebrity", k=3), [])

        start = time.perf_counter()
        for i in range(20):
            recommend_fast(graph, f"fan{i:05d}", k=5)
        elapsed = time.perf_counter() - start
        self.assertLess(elapsed, 1.5, f"20 hub-neighbour lookups took {elapsed:.2f}s")

if __name__ == "__main__":
    unittest.main(verbosity=2)
