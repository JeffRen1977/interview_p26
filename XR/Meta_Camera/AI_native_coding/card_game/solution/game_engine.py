"""Reference solution — rule validation and round settlement."""

from __future__ import annotations

from collections import Counter
from typing import Optional, Sequence

from models.card import Card
from table import Table

TARGET_SUM = 15
SET_SIZE = 3


class InvalidMove(ValueError):
    """Raised when apply_move is given a move the rules reject."""


class GameEngine:
    def __init__(self, table: Table) -> None:
        self.table = table
        self.score = 0

    def is_valid_move(self, cards: Sequence[Card]) -> bool:
        """Phase 1 fix.

        The buggy version compared *ranks* against a list of table ranks, which
        broke two rules at once:
          1. a card that is not on the table passes as long as some card shares
             its rank (7H sneaks in behind 7S);
          2. the same card can be counted three times, because plain membership
             says nothing about how many copies exist.
        Both disappear once the check becomes multiset containment, which Table
        already exposes as `contains_all`.
        """
        if len(cards) != SET_SIZE:
            return False
        if sum(card.rank for card in cards) != TARGET_SUM:
            return False
        return self.table.contains_all(cards)

    def apply_move(self, cards: Sequence[Card]) -> int:
        if not self.is_valid_move(cards):
            raise InvalidMove(f"illegal move: {[str(c) for c in cards]}")
        self.table.remove(cards)
        self.score += 1
        return self.score

    # ------------------------------------------------------------------
    # Phase 2
    # ------------------------------------------------------------------
    def play_out(self, strategy, max_moves: Optional[int] = None) -> int:
        """Settle the table until the strategy runs out of legal moves."""
        moves = 0
        while max_moves is None or moves < max_moves:
            move = strategy.choose_move(self.table)
            if move is None:
                break
            if not self.is_valid_move(move):
                raise InvalidMove(f"strategy proposed an illegal move: {[str(c) for c in move]}")
            self.apply_move(move)
            moves += 1
        return self.score

    def counts(self) -> Counter:
        return self.table.counts()
