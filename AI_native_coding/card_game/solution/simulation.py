"""Reference solution — Monte-Carlo evaluation of a strategy."""

from __future__ import annotations

import random
from typing import Dict, List

from game_engine import GameEngine
from models.card import SUITS, Card
from strategy import Strategy
from table import Table


def deal(rng: random.Random, size: int) -> List[Card]:
    """Deterministic random table given an rng seeded by the caller."""
    return [Card(rng.randint(1, 9), rng.choice(SUITS)) for _ in range(size)]


def monte_carlo(strategy: Strategy, games: int, table_size: int, seed: int) -> Dict[str, float]:
    """Run `games` independent tables and report score statistics.

    One rng seeded once, drained in a fixed order, so the same seed always
    replays the same tables. Never call random.* at module level or use the
    global rng: the determinism assertion is part of the spec.
    """
    rng = random.Random(seed)
    scores: List[int] = []
    for _ in range(games):
        table = Table(deal(rng, table_size))
        scores.append(GameEngine(table).play_out(strategy))

    total = sum(scores)
    return {
        "games": games,
        "total_score": total,
        "mean_score": total / games if games else 0.0,
        "max_score": max(scores) if scores else 0,
        "zero_score_games": sum(1 for s in scores if s == 0),
    }
