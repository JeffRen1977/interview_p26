"""Rule validation and round settlement.

Phase 1 lives here: `is_valid_move` has a logic hole. Find it from the failing
assertions before you touch anything.
"""

from __future__ import annotations

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
        """A move is legal iff it is exactly SET_SIZE cards, every one of them is
        actually available on the table, and their ranks sum to TARGET_SUM.
        """
        if len(cards) != SET_SIZE:
            return False
        if sum(card.rank for card in cards) != TARGET_SUM:
            return False
        table_ranks = [card.rank for card in self.table.cards]
        for card in cards:
            if card.rank not in table_ranks:
                return False
        return True

    def apply_move(self, cards: Sequence[Card]) -> int:
        """Validate, remove the cards from the table, and award one point."""
        if not self.is_valid_move(cards):
            raise InvalidMove(f"illegal move: {[str(c) for c in cards]}")
        self.table.remove(cards)
        self.score += 1
        return self.score

    # ------------------------------------------------------------------
    # Phase 2
    # ------------------------------------------------------------------
    def play_out(self, strategy, max_moves: Optional[int] = None) -> int:
        """Ask the strategy for moves until it has none left; return total score.

        TODO(Phase 2): loop `strategy.choose_move(self.table)`, validate and
        apply each move, stop when it returns None (or max_moves is reached).
        """
        raise NotImplementedError("Phase 2: implement play_out")
