"""Reference solution — path finding."""

from __future__ import annotations

from collections import deque
from typing import Dict, List, Optional, Set, Tuple

from grid import KEYS, Coord, key_bit
from maze import Maze


def dfs_reachable(maze: Maze, start: Coord, goal: Coord, visited: Optional[Set[Coord]] = None) -> bool:
    """Phase 1 fix: without `visited`, two adjacent open cells recurse forever.

    The stack overflow was never about maze size — a 3-cell corridor is enough.
    """
    if visited is None:
        visited = set()
    if start == goal:
        return True
    visited.add(start)
    for nxt in maze.neighbors(start):
        if nxt not in visited and dfs_reachable(maze, nxt, goal, visited):
            return True
    return False


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def shortest_path(maze: Maze) -> Optional[List[Coord]]:
    """BFS. On an unweighted grid the first time BFS pops the goal it is optimal;
    DFS would return *a* path, which is not the same thing.
    """
    start, end = maze.start, maze.end
    if start == end:
        return [start]

    parent: Dict[Coord, Optional[Coord]] = {start: None}
    queue = deque([start])
    while queue:
        current = queue.popleft()
        for nxt in maze.neighbors(current):
            if nxt in parent:
                continue
            parent[nxt] = current
            if nxt == end:
                return _rebuild(parent, end)
            queue.append(nxt)
    return None


def _rebuild(parent: Dict[Coord, Optional[Coord]], end: Coord) -> List[Coord]:
    path: List[Coord] = []
    node: Optional[Coord] = end
    while node is not None:
        path.append(node)
        node = parent[node]
    path.reverse()
    return path


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
State = Tuple[Coord, int]


def shortest_path_all_keys(maze: Maze) -> Optional[List[Coord]]:
    """Bitmask BFS over (cell, keys_mask).

    Why the naive approach fails: "BFS to the nearest key, then the next" is a
    greedy ordering and is not optimal, and enumerating all k! key orders with
    pairwise BFS blows up. Folding the key set into the state keeps one BFS,
    O(rows * cols * 2^k) states, and it is still shortest-path-optimal because
    every edge costs 1.

    Visited MUST be keyed by (cell, mask). Keying it by cell alone prunes the
    revisit that carries a new key, which is exactly how the only legal route
    disappears.
    """
    start, end = maze.start, maze.end
    goal_mask = maze.all_keys_mask

    start_mask = _pickup(maze, start, 0)
    if start == end and start_mask == goal_mask:
        return [start]

    parent: Dict[State, Optional[State]] = {(start, start_mask): None}
    queue = deque([(start, start_mask)])
    while queue:
        coord, mask = queue.popleft()
        for nxt in maze.neighbors(coord, mask):
            nxt_mask = _pickup(maze, nxt, mask)
            state = (nxt, nxt_mask)
            if state in parent:
                continue
            parent[state] = (coord, mask)
            if nxt == end and nxt_mask == goal_mask:
                return [cell for cell, _ in _rebuild_states(parent, state)]
            queue.append(state)
    return None


def _pickup(maze: Maze, coord: Coord, mask: int) -> int:
    char = maze.at(coord)
    if char in KEYS:
        return mask | (1 << key_bit(char))
    return mask


def _rebuild_states(parent: Dict[State, Optional[State]], end: State) -> List[State]:
    path: List[State] = []
    node: Optional[State] = end
    while node is not None:
        path.append(node)
        node = parent[node]
    path.reverse()
    return path
