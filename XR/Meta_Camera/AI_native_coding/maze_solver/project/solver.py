"""Path finding.

Phase 1: dfs_reachable blows the stack — something is missing.
Phase 2: shortest_path — unweighted shortest path, not "any path".
Phase 3: shortest_path_all_keys — keys are mandatory checkpoints and gates
         enforce the order, so the state is no longer just a cell.
"""

from __future__ import annotations

from typing import List, Optional

from grid import Coord
from maze import Maze


def dfs_reachable(maze: Maze, start: Coord, goal: Coord) -> bool:
    """Can we walk from start to goal ignoring keys and gates?"""
    if start == goal:
        return True
    for nxt in maze.neighbors(start):
        if dfs_reachable(maze, nxt, goal):
            return True
    return False


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def shortest_path(maze: Maze) -> Optional[List[Coord]]:
    """Fewest-cells path from maze.start to maze.end, ignoring keys/gates.

    Returns the list of cells including both endpoints, or None if unreachable.
    A maze whose start equals its end returns [start].

    TODO(Phase 2): BFS with a visited set and a parent map for reconstruction.
    """
    raise NotImplementedError("Phase 2: implement shortest_path")


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
def shortest_path_all_keys(maze: Maze) -> Optional[List[Coord]]:
    """Shortest walk that collects EVERY key and then stands on maze.end.

    Gates ('A'..'F') are walls until the matching key ('a'..'f') is collected.
    Cells may be revisited — with a different key set they are different states.

    TODO(Phase 3): BFS over (row, col, keys_mask). Visited must be keyed by the
    full state, not by the cell, or you will prune the only legal route.
    """
    raise NotImplementedError("Phase 3: implement shortest_path_all_keys")
