"""Move-selection strategies.

Phase 2: GreedyStrategy — find any legal sum-15 triplet, brute force is fine.
Phase 3: count_triplets_sum_15 + FastStrategy — the brute force dies on a
         2,000-card table, especially when *no* triplet exists and it has to
         enumerate every combination to prove it.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import List, Optional, Sequence, Tuple

from models.card import Card

TARGET_SUM = 15
SET_SIZE = 3

Triplet = Tuple[Card, Card, Card]


class Strategy(ABC):
    """Contract used by GameEngine.play_out. Do not change this signature."""

    @abstractmethod
    def choose_move(self, table) -> Optional[Triplet]:
        """Return one legal triplet from the table, or None if there is none."""


class GreedyStrategy(Strategy):
    """Phase 2: first legal triplet in table order."""

    def choose_move(self, table) -> Optional[Triplet]:
        # TODO(Phase 2): return the first combination of 3 distinct positions
        # on the table whose ranks sum to TARGET_SUM, else None.
        raise NotImplementedError("Phase 2: implement GreedyStrategy.choose_move")


def count_triplets_sum_15(cards: Sequence[Card]) -> int:
    """Phase 3: how many distinct 3-card subsets (by position) sum to 15.

    Must stay fast for 20,000+ cards, so O(n^3) enumeration is out.
    Hint: ranks only span 1..9.
    """
    # TODO(Phase 3): frequency table over ranks + combinatorics.
    raise NotImplementedError("Phase 3: implement count_triplets_sum_15")


class FastStrategy(Strategy):
    """Phase 3: same behaviour as GreedyStrategy, but O(9^3) per decision."""

    def choose_move(self, table) -> Optional[Triplet]:
        # TODO(Phase 3): bucket cards by rank, scan rank triples r1<=r2<=r3
        # summing to 15, and materialise one triplet from the buckets.
        raise NotImplementedError("Phase 3: implement FastStrategy.choose_move")


def all_triplet_ranks() -> List[Tuple[int, int, int]]:
    """Every non-decreasing rank triple (r1<=r2<=r3) in 1..9 summing to 15."""
    out: List[Tuple[int, int, int]] = []
    for r1 in range(1, 10):
        for r2 in range(r1, 10):
            r3 = TARGET_SUM - r1 - r2
            if r2 <= r3 <= 9:
                out.append((r1, r2, r3))
    return out
