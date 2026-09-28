"""The pool of cards face-up on the table. Do not change this file."""

from __future__ import annotations

from collections import Counter
from typing import Iterable, List, Sequence

from models.card import Card


class Table:
    """A multiset of cards.

    The same (rank, suit) can appear more than once, so every containment or
    removal check must be done on *counts*, not on plain membership.
    """

    def __init__(self, cards: Iterable[Card] = ()) -> None:
        self._cards: List[Card] = list(cards)

    @property
    def cards(self) -> List[Card]:
        """A copy — callers must not mutate the table through this list."""
        return list(self._cards)

    def counts(self) -> Counter:
        """Multiset view: Card -> how many copies are on the table."""
        return Counter(self._cards)

    def contains_all(self, cards: Sequence[Card]) -> bool:
        """True only if the table holds enough copies of every requested card.

        `contains_all([c, c])` is False when only one copy of `c` is on the table.
        """
        need = Counter(cards)
        have = self.counts()
        return all(have[card] >= n for card, n in need.items())

    def remove(self, cards: Sequence[Card]) -> None:
        if not self.contains_all(cards):
            raise ValueError("cannot remove cards that are not on the table")
        for card in cards:
            self._cards.remove(card)

    def add(self, cards: Iterable[Card]) -> None:
        self._cards.extend(cards)

    def __len__(self) -> int:
        return len(self._cards)

    def __repr__(self) -> str:
        return f"Table({[str(c) for c in self._cards]})"
