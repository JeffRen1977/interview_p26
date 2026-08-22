"""Maze model: walls, start/end, keys and gates. Do not change this file."""

from __future__ import annotations

from typing import Dict, Iterator, List, Optional

from grid import DIRECTIONS, END, GATES, KEYS, START, WALL, Coord, key_bit, parse_grid


class Maze:
    """A rectangular grid.

    Legend: '#' wall, '.' open, 'S' start, 'E' end,
            'a'..'f' key, 'A'..'F' the gate that key opens.
    """

    def __init__(self, grid: List[List[str]]) -> None:
        self.grid = grid
        self.rows = len(grid)
        self.cols = len(grid[0])
        self.start: Optional[Coord] = None
        self.end: Optional[Coord] = None
        self.keys: Dict[str, Coord] = {}
        self.gates: Dict[str, Coord] = {}

        for r, row in enumerate(grid):
            for c, char in enumerate(row):
                here = Coord(r, c)
                if char == START:
                    self.start = here
                elif char == END:
                    self.end = here
                elif char in KEYS:
                    self.keys[char] = here
                elif char in GATES:
                    self.gates[char] = here

        if self.start is None or self.end is None:
            raise ValueError("maze needs both a start (S) and an end (E)")

    @classmethod
    def from_text(cls, text: str) -> "Maze":
        return cls(parse_grid(text))

    def at(self, coord: Coord) -> str:
        return self.grid[coord.row][coord.col]

    def in_bounds(self, coord: Coord) -> bool:
        return 0 <= coord.row < self.rows and 0 <= coord.col < self.cols

    def is_wall(self, coord: Coord) -> bool:
        return self.at(coord) == WALL

    def is_open(self, coord: Coord, keys_mask: int = 0) -> bool:
        """Passable given the keys collected so far.

        A gate is a wall until its key is in the mask.
        """
        if not self.in_bounds(coord) or self.is_wall(coord):
            return False
        char = self.at(coord)
        if char in GATES:
            return bool(keys_mask & (1 << key_bit(char)))
        return True

    def neighbors(self, coord: Coord, keys_mask: int = 0) -> Iterator[Coord]:
        """Passable 4-neighbours, in DIRECTIONS order."""
        for dr, dc in DIRECTIONS:
            nxt = coord.shifted(dr, dc)
            if self.is_open(nxt, keys_mask):
                yield nxt

    @property
    def all_keys_mask(self) -> int:
        mask = 0
        for char in self.keys:
            mask |= 1 << key_bit(char)
        return mask
