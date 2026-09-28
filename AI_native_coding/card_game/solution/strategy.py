"""Reference solution — move-selection strategies."""

from __future__ import annotations

from abc import ABC, abstractmethod
from collections import Counter, defaultdict
from itertools import combinations
from math import comb
from typing import Dict, List, Optional, Sequence, Tuple

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
    """Phase 2: first legal triplet in table order.

    O(n^3) — fine for a 20-card table, fatal for 2,000 cards, and worst exactly
    when the answer is None because every combination must be enumerated to
    prove it. That is the wall Phase 3 removes.
    """

    def choose_move(self, table) -> Optional[Triplet]:
        cards = table.cards
        for triplet in combinations(cards, SET_SIZE):
            if sum(card.rank for card in triplet) == TARGET_SUM:
                return triplet
        return None


def count_triplets_sum_15(cards: Sequence[Card]) -> int:
    """Phase 3: number of distinct 3-card subsets (by position) summing to 15.

    Ranks live in 1..9, so instead of enumerating C(n, 3) card triples we
    enumerate the <=84 non-decreasing rank triples and expand each one with
    binomial coefficients over the rank frequencies: O(n + 9^2).
    """
    freq: Counter = Counter(card.rank for card in cards)
    total = 0
    for r1, r2, r3 in all_triplet_ranks():
        if r1 == r2 == r3:
            total += comb(freq[r1], 3)
        elif r1 == r2:
            total += comb(freq[r1], 2) * freq[r3]
        elif r2 == r3:
            total += freq[r1] * comb(freq[r2], 2)
        else:
            total += freq[r1] * freq[r2] * freq[r3]
    return total


class FastStrategy(Strategy):
    """Phase 3: bucket by rank, then scan the fixed rank-triple table.

    Each decision costs O(n) to bucket plus O(84) to scan, independent of how
    many combinations the table admits — and proving "no move" is just as cheap
    as finding one.
    """

    def choose_move(self, table) -> Optional[Triplet]:
        buckets: Dict[int, List[Card]] = defaultdict(list)
        for card in table.cards:
            buckets[card.rank].append(card)

        for r1, r2, r3 in all_triplet_ranks():
            need: Counter = Counter((r1, r2, r3))
            if all(len(buckets[rank]) >= n for rank, n in need.items()):
                picked: List[Card] = []
                for rank, n in need.items():
                    picked.extend(buckets[rank][:n])
                return tuple(picked)  # type: ignore[return-value]
        return None


def all_triplet_ranks() -> List[Tuple[int, int, int]]:
    """Every non-decreasing rank triple (r1<=r2<=r3) in 1..9 summing to 15."""
    out: List[Tuple[int, int, int]] = []
    for r1 in range(1, 10):
        for r2 in range(r1, 10):
            r3 = TARGET_SUM - r1 - r2
            if r2 <= r3 <= 9:
                out.append((r1, r2, r3))
    return out
