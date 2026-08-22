"""Phase 3: Monte-Carlo evaluation of a strategy over many random tables."""

from __future__ import annotations

import random
from typing import Dict, List

from game_engine import GameEngine
from models.card import SUITS, Card
from strategy import Strategy


def deal(rng: random.Random, size: int) -> List[Card]:
    """Deterministic random table given an rng seeded by the caller."""
    return [Card(rng.randint(1, 9), rng.choice(SUITS)) for _ in range(size)]


def monte_carlo(strategy: Strategy, games: int, table_size: int, seed: int) -> Dict[str, float]:
    """Run `games` independent tables and report score statistics.

    Must be deterministic for a given seed: same seed -> identical dict.

    TODO(Phase 3): build a random.Random(seed), deal each table, play it out
    with the strategy, and return {"games", "total_score", "mean_score",
    "max_score", "zero_score_games"}.
    """
    raise NotImplementedError("Phase 3: implement monte_carlo")
