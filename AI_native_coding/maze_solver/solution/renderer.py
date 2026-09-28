"""Reference solution — maze rendering."""

from __future__ import annotations

from typing import Iterable, Optional

from grid import OPEN, PATH, Coord
from maze import Maze


def render(maze: Maze, path: Optional[Iterable[Coord]] = None) -> str:
    """Phase 1 fix: index rows first, then columns.

    The buggy version wrote grid[coord.col][coord.row]. On a square maze that
    silently draws the transposed path; on a rectangular one it walks off the
    end of the row list and raises IndexError.
    """
    grid = [row[:] for row in maze.grid]
    for coord in path or ():
        if grid[coord.row][coord.col] == OPEN:
            grid[coord.row][coord.col] = PATH
    return "\n".join("".join(row) for row in grid)
