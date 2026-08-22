"""Grid parsing and coordinates. Do not change this file during the interview."""

from __future__ import annotations

from typing import List, NamedTuple

WALL = "#"
OPEN = "."
START = "S"
END = "E"
PATH = "*"

#: Gates stop at D on purpose: 'E' is already the exit marker.
KEYS = "abcd"
GATES = "ABCD"


class Coord(NamedTuple):
    """(row, col) — row first, always. Mixing the two up is the classic bug."""

    row: int
    col: int

    def shifted(self, dr: int, dc: int) -> "Coord":
        return Coord(self.row + dr, self.col + dc)


#: Neighbour order is part of the contract: up, down, left, right.
DIRECTIONS = ((-1, 0), (1, 0), (0, -1), (0, 1))


def parse_grid(text: str) -> List[List[str]]:
    """Parse a maze drawing into a rectangular grid of single characters.

    Blank lines are ignored. Every row must have the same width.
    """
    rows = [list(line) for line in text.strip("\n").splitlines() if line.strip()]
    if not rows:
        raise ValueError("empty maze")
    width = len(rows[0])
    if any(len(row) != width for row in rows):
        raise ValueError("maze is not rectangular")
    return rows


def key_bit(char: str) -> int:
    """Bit index for a key/gate letter: a/A -> 0, b/B -> 1, ..."""
    return ord(char.lower()) - ord("a")
