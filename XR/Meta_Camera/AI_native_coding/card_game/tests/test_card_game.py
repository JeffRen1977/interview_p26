"""Staged spec for the Card Game task. These tests are the contract.

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
from collections import Counter
from itertools import combinations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

from game_engine import SET_SIZE, TARGET_SUM, GameEngine, InvalidMove  # noqa: E402
from models.card import Card  # noqa: E402
from simulation import monte_carlo  # noqa: E402
from strategy import (  # noqa: E402
    FastStrategy,
    GreedyStrategy,
    count_triplets_sum_15,
    all_triplet_ranks,
)
from table import Table  # noqa: E402


def C(rank: int, suit: str = "S") -> Card:
    return Card(rank, suit)


def brute_force_count(cards):
    return sum(1 for t in combinations(cards, SET_SIZE) if sum(c.rank for c in t) == TARGET_SUM)


def has_any_triplet(cards):
    return any(sum(c.rank for c in t) == TARGET_SUM for t in combinations(cards, SET_SIZE))


# ----------------------------------------------------------------------
# Phase 1 — architecture warm-up and bug fix
# ----------------------------------------------------------------------
class Phase1RuleValidation(unittest.TestCase):
    def setUp(self) -> None:
        self.table = Table([C(7, "S"), C(3, "H"), C(5, "C"), C(9, "D"), C(1, "S")])
        self.engine = GameEngine(self.table)

    def test_legal_triplet_is_accepted(self) -> None:
        self.assertTrue(self.engine.is_valid_move([C(7, "S"), C(3, "H"), C(5, "C")]))

    def test_wrong_sum_is_rejected(self) -> None:
        self.assertFalse(self.engine.is_valid_move([C(7, "S"), C(3, "H"), C(9, "D")]))

    def test_wrong_size_is_rejected(self) -> None:
        self.assertFalse(self.engine.is_valid_move([C(7, "S"), C(3, "H")]))
        self.assertFalse(self.engine.is_valid_move([C(7, "S"), C(3, "H"), C(5, "C"), C(1, "S")]))

    def test_card_not_on_the_table_is_rejected(self) -> None:
        """7H has the same rank as the 7S on the table, but it is a different card."""
        self.assertFalse(self.engine.is_valid_move([C(7, "H"), C(3, "H"), C(5, "C")]))

    def test_same_card_cannot_be_used_twice(self) -> None:
        """Only one 5C is on the table, so 5C+5C+5C must not be legal."""
        self.assertFalse(self.engine.is_valid_move([C(5, "C"), C(5, "C"), C(5, "C")]))

    def test_duplicate_is_legal_when_the_table_really_holds_two_copies(self) -> None:
        table = Table([C(5, "C"), C(5, "C"), C(5, "H")])
        engine = GameEngine(table)
        self.assertTrue(engine.is_valid_move([C(5, "C"), C(5, "C"), C(5, "H")]))

    def test_apply_move_scores_and_removes_exactly_those_cards(self) -> None:
        self.engine.apply_move([C(7, "S"), C(3, "H"), C(5, "C")])
        self.assertEqual(self.engine.score, 1)
        self.assertEqual(sorted(self.table.cards), sorted([C(1, "S"), C(9, "D")]))

    def test_apply_move_rejects_an_illegal_move(self) -> None:
        with self.assertRaises(InvalidMove):
            self.engine.apply_move([C(5, "C"), C(5, "C"), C(5, "C")])
        self.assertEqual(self.engine.score, 0)
        self.assertEqual(len(self.table), 5)


# ----------------------------------------------------------------------
# Phase 2 — baseline strategy and settlement loop
# ----------------------------------------------------------------------
class Phase2GreedyStrategy(unittest.TestCase):
    def setUp(self) -> None:
        self.strategy = GreedyStrategy()

    def test_choose_move_returns_a_legal_triplet(self) -> None:
        table = Table([C(9, "S"), C(2, "H"), C(4, "C"), C(8, "D")])
        move = self.strategy.choose_move(table)
        self.assertIsNotNone(move)
        self.assertEqual(len(move), SET_SIZE)
        self.assertTrue(GameEngine(table).is_valid_move(move))

    def test_choose_move_returns_none_when_no_set_exists(self) -> None:
        table = Table([C(9, "S"), C(9, "H"), C(9, "C"), C(8, "D")])
        self.assertIsNone(self.strategy.choose_move(table))

    def test_choose_move_on_empty_table(self) -> None:
        self.assertIsNone(self.strategy.choose_move(Table([])))

    def test_play_out_clears_one_set(self) -> None:
        table = Table([C(7, "S"), C(3, "H"), C(5, "C"), C(9, "D")])
        engine = GameEngine(table)
        self.assertEqual(engine.play_out(self.strategy), 1)
        self.assertEqual(sorted(table.cards), [C(9, "D")])

    def test_play_out_clears_two_disjoint_sets(self) -> None:
        table = Table([C(1, "S"), C(6, "H"), C(8, "C"), C(2, "D"), C(6, "S"), C(7, "H")])
        engine = GameEngine(table)
        self.assertEqual(engine.play_out(self.strategy), 2)
        self.assertEqual(len(table), 0)

    def test_play_out_scores_zero_when_nothing_is_playable(self) -> None:
        table = Table([C(9, "S"), C(9, "H"), C(1, "C")])
        engine = GameEngine(table)
        self.assertEqual(engine.play_out(self.strategy), 0)
        self.assertEqual(len(table), 3)

    def test_play_out_respects_max_moves(self) -> None:
        table = Table([C(5, "S"), C(5, "H"), C(5, "C"), C(5, "D"), C(5, "S"), C(5, "H")])
        engine = GameEngine(table)
        self.assertEqual(engine.play_out(self.strategy, max_moves=1), 1)
        self.assertEqual(len(table), 3)

    def test_play_out_conserves_cards_and_reaches_a_terminal_state(self) -> None:
        rng = random.Random(7)
        for _ in range(20):
            cards = [C(rng.randint(1, 9), rng.choice("SHDC")) for _ in range(14)]
            table = Table(cards)
            engine = GameEngine(table)
            score = engine.play_out(self.strategy)
            remaining = table.cards
            # no card was invented, duplicated or lost
            self.assertEqual(len(remaining), len(cards) - SET_SIZE * score)
            consumed = Counter(cards) - Counter(remaining)
            self.assertEqual(sum(consumed.values()), SET_SIZE * score)
            self.assertEqual(Counter(remaining) + consumed, Counter(cards))
            self.assertFalse(has_any_triplet(remaining), "strategy stopped with a set still playable")


# ----------------------------------------------------------------------
# Phase 3 — scale, adversarial input and Monte-Carlo simulation
# ----------------------------------------------------------------------
class Phase3ScaleAndEdges(unittest.TestCase):
    def test_rank_triples_helper(self) -> None:
        triples = all_triplet_ranks()
        self.assertIn((1, 5, 9), triples)
        self.assertIn((5, 5, 5), triples)
        self.assertTrue(all(sum(t) == TARGET_SUM for t in triples))

    def test_count_matches_brute_force_on_random_tables(self) -> None:
        rng = random.Random(11)
        for _ in range(25):
            cards = [C(rng.randint(1, 9), rng.choice("SHDC")) for _ in range(rng.randint(0, 18))]
            self.assertEqual(count_triplets_sum_15(cards), brute_force_count(cards))

    def test_count_on_edge_inputs(self) -> None:
        self.assertEqual(count_triplets_sum_15([]), 0)
        self.assertEqual(count_triplets_sum_15([C(5), C(5)]), 0)
        self.assertEqual(count_triplets_sum_15([C(5), C(5), C(5)]), 1)
        self.assertEqual(count_triplets_sum_15([C(5)] * 6), 20)  # C(6,3)
        self.assertEqual(count_triplets_sum_15([C(9)] * 100), 0)

    def test_count_is_fast_on_a_huge_table(self) -> None:
        rng = random.Random(3)
        cards = [C(rng.randint(1, 9), rng.choice("SHDC")) for _ in range(20000)]
        start = time.perf_counter()
        total = count_triplets_sum_15(cards)
        elapsed = time.perf_counter() - start
        self.assertGreater(total, 0)
        self.assertLess(elapsed, 1.0, f"count_triplets_sum_15 took {elapsed:.2f}s on 20k cards")

    def test_fast_strategy_picks_legal_moves(self) -> None:
        rng = random.Random(5)
        strategy = FastStrategy()
        for _ in range(20):
            cards = [C(rng.randint(1, 9), rng.choice("SHDC")) for _ in range(12)]
            table = Table(cards)
            move = strategy.choose_move(table)
            if move is None:
                self.assertFalse(has_any_triplet(cards))
            else:
                self.assertTrue(GameEngine(table).is_valid_move(move))

    def test_fast_strategy_reaches_a_terminal_state(self) -> None:
        rng = random.Random(13)
        strategy = FastStrategy()
        for _ in range(20):
            cards = [C(rng.randint(1, 9), rng.choice("SHDC")) for _ in range(14)]
            table = Table(cards)
            GameEngine(table).play_out(strategy)
            self.assertFalse(has_any_triplet(table.cards))

    def test_fast_strategy_survives_the_adversarial_table(self) -> None:
        """2,000 nines (no triplet among them) hiding nine fives at the end.

        Brute force has to enumerate C(2009, 3) combinations to prove the nines
        are dead, which is why Phase 2's strategy times out here.
        """
        cards = [C(9, "S")] * 2000 + [C(5, "H")] * 9
        table = Table(cards)
        engine = GameEngine(table)
        start = time.perf_counter()
        score = engine.play_out(FastStrategy())
        elapsed = time.perf_counter() - start
        self.assertEqual(score, 3)
        self.assertEqual(len(table), 2000)
        self.assertLess(elapsed, 5.0, f"play_out took {elapsed:.2f}s on a 2k-card table")

    def test_monte_carlo_is_deterministic_for_a_seed(self) -> None:
        a = monte_carlo(FastStrategy(), games=40, table_size=20, seed=42)
        b = monte_carlo(FastStrategy(), games=40, table_size=20, seed=42)
        self.assertEqual(a, b)
        self.assertEqual(a["games"], 40)
        self.assertGreater(a["total_score"], 0)
        self.assertAlmostEqual(a["mean_score"], a["total_score"] / 40)
        self.assertGreaterEqual(a["max_score"], a["mean_score"])
        self.assertIn("zero_score_games", a)

    def test_monte_carlo_handles_a_table_too_small_to_score(self) -> None:
        stats = monte_carlo(FastStrategy(), games=5, table_size=2, seed=1)
        self.assertEqual(stats["total_score"], 0)
        self.assertEqual(stats["zero_score_games"], 5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
