"""Maze rendering.

Phase 1 lives here too: the path overlay comes out transposed.
"""

from __future__ import annotations

from typing import Iterable, Optional

from grid import OPEN, PATH, Coord
from maze import Maze


def render(maze: Maze, path: Optional[Iterable[Coord]] = None) -> str:
    """Draw the maze, overlaying '*' on every open cell the path walks through.

    S, E, keys and gates keep their own character — the path must not hide them.
    """
    grid = [row[:] for row in maze.grid]
    for coord in path or ():
        if grid[coord.col][coord.row] == OPEN:
            grid[coord.col][coord.row] = PATH
    return "\n".join("".join(row) for row in grid)
