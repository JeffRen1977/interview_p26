"""Immutable playing card. Do not change this file during the interview."""

from __future__ import annotations

from dataclasses import dataclass

SUITS = ("S", "H", "D", "C")
MIN_RANK = 1
MAX_RANK = 9


@dataclass(frozen=True, order=True)
class Card:
    """A card with a rank in [1, 9] and a suit.

    Frozen + ordered so cards are hashable, sortable and comparable by value.
    Two cards with the same (rank, suit) are equal — the table may legitimately
    hold several of them because the shoe is built from multiple decks.
    """

    rank: int
    suit: str

    def __post_init__(self) -> None:
        if not MIN_RANK <= self.rank <= MAX_RANK:
            raise ValueError(f"rank must be in [{MIN_RANK}, {MAX_RANK}], got {self.rank}")
        if self.suit not in SUITS:
            raise ValueError(f"suit must be one of {SUITS}, got {self.suit!r}")

    def __str__(self) -> str:
        return f"{self.rank}{self.suit}"
