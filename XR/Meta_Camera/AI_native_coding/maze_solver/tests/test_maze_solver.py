"""Staged spec for the Maze Solver task. These tests are the contract.

Run against the interview skeleton (project/, starts red):
    python3 -m unittest discover -s tests -v

Run against the reference implementation (solution/, must be green):
    AINC_IMPL=solution python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import os
import sys
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

from grid import BOMB_COST, GATES, KEYS, Coord, key_bit  # noqa: E402
from maze import Maze  # noqa: E402
from renderer import render  # noqa: E402
from solver import (  # noqa: E402
    dfs_reachable,
    min_energy_path,
    shortest_path,
    shortest_path_all_keys,
)

# --- fixtures ---------------------------------------------------------
SQUARE = """\
#####
#S..#
#.#.#
#..E#
#####"""

CORRIDOR = """\
#######
#S...E#
#######"""

BLOCKED = """\
#####
#S#E#
#####"""

TWO_KEYS = """\
########
#S.....#
#.####.#
#a....b#
#.####.#
#..A..E#
########"""

GATE_ON_THE_ONLY_ROUTE = """\
#######
#S.A.E#
#.#####
#a....#
#######"""

GATE_WITHOUT_ITS_KEY = """\
#######
#S.A.E#
#.#####
#.....#
#######"""


#: Straight line through rough terrain vs a longer detour on plain floor.
ROUGH_DETOUR = """\
#######
#S~~~E#
#.....#
#######"""

#: One interior wall on the only route.
ONE_WALL = """\
#######
#S.#.E#
#######"""

#: Two interior walls: one bomb is not enough.
TWO_WALLS = """\
#######
#S#.#E#
#######"""

#: Detour is 12 energy, blasting through costs 11 — the bomb is worth it.
BOMB_BEATS_DETOUR = """\
###########
#S...#...E#
#.#######.#
#.........#
###########"""

#: Same shape, short detour (6) beats the blast (7) — the bomb must stay unused.
DETOUR_BEATS_BOMB = """\
#######
#S.#.E#
#.....#
#######"""


def big_maze(size: int = 81, with_keys: bool = True) -> Maze:
    """An open size x size room with a wall border, S top-left, E bottom-right."""
    rows = [["#"] * size for _ in range(size)]
    for r in range(1, size - 1):
        for c in range(1, size - 1):
            rows[r][c] = "."
    rows[1][1] = "S"
    rows[size - 2][size - 2] = "E"
    if with_keys:
        rows[1][size - 2] = "a"
        rows[size - 2][1] = "b"
        rows[size // 2][1] = "c"
        rows[size // 2][size - 2] = "d"
    return Maze(rows)


class MazeAssertions(unittest.TestCase):
    def assert_valid_walk(self, maze: Maze, path, require_all_keys: bool = False) -> None:
        self.assertIsNotNone(path, "expected a path, got None")
        self.assertEqual(path[0], maze.start)
        self.assertEqual(path[-1], maze.end)
        mask = 0
        for i, cell in enumerate(path):
            self.assertTrue(maze.in_bounds(cell), f"{cell} out of bounds")
            self.assertFalse(maze.is_wall(cell), f"walked into a wall at {cell}")
            char = maze.at(cell)
            if char in GATES:
                self.assertTrue(mask & (1 << key_bit(char)), f"passed gate {char} without its key")
            if char in KEYS:
                mask |= 1 << key_bit(char)
            if i:
                prev = path[i - 1]
                step = abs(prev.row - cell.row) + abs(prev.col - cell.col)
                self.assertEqual(step, 1, f"non-adjacent step {prev} -> {cell}")
        if require_all_keys:
            self.assertEqual(mask, maze.all_keys_mask, "did not collect every key")

    def assert_valid_blast_walk(self, maze: Maze, result, bombs: int) -> None:
        """Same invariants as assert_valid_walk, plus the energy accounting.

        A wall cell is allowed in the path only if it was bombable and the
        budget covers every blast; the reported energy must equal the sum of
        the step costs the walk actually pays.
        """
        self.assertIsNotNone(result, "expected (energy, path), got None")
        energy, path = result
        self.assertEqual(path[0], maze.start)
        self.assertEqual(path[-1], maze.end)
        mask = 0
        spent = 0
        blasts = 0
        for i, cell in enumerate(path):
            self.assertTrue(maze.in_bounds(cell), f"{cell} out of bounds")
            char = maze.at(cell)
            if i:
                prev = path[i - 1]
                step = abs(prev.row - cell.row) + abs(prev.col - cell.col)
                self.assertEqual(step, 1, f"non-adjacent step {prev} -> {cell}")
                if maze.is_wall(cell):
                    self.assertTrue(maze.is_bombable(cell), f"blew up bedrock at {cell}")
                    blasts += 1
                    spent += BOMB_COST
                else:
                    if char in GATES:
                        self.assertTrue(
                            mask & (1 << key_bit(char)), f"passed gate {char} without its key"
                        )
                    spent += maze.terrain_cost(cell)
            if char in KEYS:
                mask |= 1 << key_bit(char)
        self.assertLessEqual(blasts, bombs, f"used {blasts} bombs, budget was {bombs}")
        self.assertEqual(spent, energy, "reported energy does not match the walk")
        self.assertEqual(mask, maze.all_keys_mask, "did not collect every key")


# ----------------------------------------------------------------------
# Phase 1 — rendering offset and the runaway DFS
# ----------------------------------------------------------------------
class Phase1RenderAndDfs(MazeAssertions):
    def test_render_without_a_path_is_the_original_drawing(self) -> None:
        self.assertEqual(render(Maze.from_text(SQUARE)), SQUARE)

    def test_render_marks_the_path(self) -> None:
        maze = Maze.from_text(SQUARE)
        path = [Coord(1, 1), Coord(2, 1), Coord(3, 1), Coord(3, 2), Coord(3, 3)]
        expected = "\n".join(["#####", "#S..#", "#*#.#", "#**E#", "#####"])
        self.assertEqual(render(maze, path), expected)

    def test_render_on_a_non_square_maze(self) -> None:
        """Rows and columns are not interchangeable; a 3x7 maze proves it."""
        maze = Maze.from_text(CORRIDOR)
        path = [Coord(1, c) for c in range(1, 6)]
        expected = "\n".join(["#######", "#S***E#", "#######"])
        self.assertEqual(render(maze, path), expected)

    def test_render_never_hides_start_end_keys_or_gates(self) -> None:
        maze = Maze.from_text(TWO_KEYS)
        path = [Coord(1, 1), Coord(2, 1), Coord(3, 1), Coord(3, 2)]
        drawn = render(maze, path).splitlines()
        self.assertEqual(drawn[1][1], "S")
        self.assertEqual(drawn[3][1], "a")
        self.assertEqual(drawn[5][6], "E")
        self.assertEqual(drawn[5][3], "A")
        self.assertEqual(drawn[3][2], "*")

    def test_dfs_reachable_terminates_on_a_loop_free_answer(self) -> None:
        maze = Maze.from_text(SQUARE)
        self.assertTrue(dfs_reachable(maze, maze.start, maze.end))

    def test_dfs_reachable_returns_false_instead_of_recursing_forever(self) -> None:
        maze = Maze.from_text(BLOCKED)
        self.assertFalse(dfs_reachable(maze, maze.start, maze.end))

    def test_dfs_start_equals_goal(self) -> None:
        maze = Maze.from_text(SQUARE)
        self.assertTrue(dfs_reachable(maze, maze.start, maze.start))


# ----------------------------------------------------------------------
# Phase 2 — shortest path, not just any path
# ----------------------------------------------------------------------
class Phase2ShortestPath(MazeAssertions):
    def test_shortest_path_length_on_a_branching_maze(self) -> None:
        maze = Maze.from_text(SQUARE)
        path = shortest_path(maze)
        self.assert_valid_walk(maze, path)
        self.assertEqual(len(path), 5)

    def test_shortest_path_in_a_corridor(self) -> None:
        maze = Maze.from_text(CORRIDOR)
        path = shortest_path(maze)
        self.assert_valid_walk(maze, path)
        self.assertEqual(len(path), 5)

    def test_unreachable_end_returns_none(self) -> None:
        self.assertIsNone(shortest_path(Maze.from_text(BLOCKED)))

    def test_start_equals_end(self) -> None:
        maze = Maze.from_text(SQUARE)
        maze.end = maze.start
        self.assertEqual(shortest_path(maze), [maze.start])

    def test_shortest_path_treats_a_closed_gate_as_a_wall(self) -> None:
        """No keys are collected by this function, so the gate never opens."""
        self.assertIsNone(shortest_path(Maze.from_text(GATE_ON_THE_ONLY_ROUTE)))

    def test_rendered_shortest_path(self) -> None:
        maze = Maze.from_text(SQUARE)
        expected = "\n".join(["#####", "#S..#", "#*#.#", "#**E#", "#####"])
        self.assertEqual(render(maze, shortest_path(maze)), expected)

    def test_shortest_path_beats_any_depth_first_walk(self) -> None:
        maze = big_maze(21, with_keys=False)
        path = shortest_path(maze)
        self.assert_valid_walk(maze, path)
        # Manhattan distance between (1,1) and (19,19) on an open grid
        self.assertEqual(len(path), 37)


# ----------------------------------------------------------------------
# Phase 3 — mandatory checkpoints, gates and scale
# ----------------------------------------------------------------------
class Phase3KeysAndScale(MazeAssertions):
    def test_collects_every_key_on_the_shortest_route(self) -> None:
        maze = Maze.from_text(TWO_KEYS)
        path = shortest_path_all_keys(maze)
        self.assert_valid_walk(maze, path, require_all_keys=True)
        self.assertEqual(len(path), 10)

    def test_gate_forces_a_detour_and_the_route_revisits_cells(self) -> None:
        """The only way out is: fetch key 'a', walk back, then open gate 'A'.

        Cells are revisited with a different key set, so `visited` keyed by cell
        alone would prune the only legal route.
        """
        maze = Maze.from_text(GATE_ON_THE_ONLY_ROUTE)
        path = shortest_path_all_keys(maze)
        self.assert_valid_walk(maze, path, require_all_keys=True)
        self.assertEqual(len(path), 9)
        self.assertLess(len(set(path)), len(path), "expected the route to revisit cells")

    def test_none_when_a_gate_can_never_be_opened(self) -> None:
        self.assertIsNone(shortest_path_all_keys(Maze.from_text(GATE_WITHOUT_ITS_KEY)))

    def test_none_when_the_end_is_walled_off(self) -> None:
        self.assertIsNone(shortest_path_all_keys(Maze.from_text(BLOCKED)))

    def test_without_keys_it_degenerates_to_plain_bfs(self) -> None:
        maze = Maze.from_text(SQUARE)
        self.assertEqual(shortest_path_all_keys(maze), shortest_path(maze))

    def test_start_equals_end_with_no_keys(self) -> None:
        maze = Maze.from_text(SQUARE)
        maze.end = maze.start
        self.assertEqual(shortest_path_all_keys(maze), [maze.start])

    def test_large_grid_with_four_keys_under_time_budget(self) -> None:
        maze = big_maze(81)
        start = time.perf_counter()
        path = shortest_path_all_keys(maze)
        elapsed = time.perf_counter() - start
        self.assert_valid_walk(maze, path, require_all_keys=True)
        self.assertLess(elapsed, 3.0, f"bitmask BFS took {elapsed:.2f}s on 81x81 with 4 keys")


# ----------------------------------------------------------------------
# Phase 4 — weighted steps and a bomb budget
# ----------------------------------------------------------------------
class Phase4EnergyAndBombs(MazeAssertions):
    def test_prefers_the_long_cheap_detour_over_rough_terrain(self) -> None:
        """BFS would take the 5-cell line through '~'; Dijkstra pays 6 for 7 cells."""
        maze = Maze.from_text(ROUGH_DETOUR)
        result = min_energy_path(maze, bombs=0)
        self.assert_valid_blast_walk(maze, result, bombs=0)
        energy, path = result
        self.assertEqual(energy, 6)
        self.assertEqual(len(path), 7)
        self.assertEqual(len(shortest_path(maze)), 5)  # the BFS answer costs 16

    def test_degenerates_to_bfs_when_every_step_costs_one(self) -> None:
        maze = Maze.from_text(SQUARE)
        result = min_energy_path(maze, bombs=0)
        self.assert_valid_blast_walk(maze, result, bombs=0)
        self.assertEqual(result[0], len(shortest_path(maze)) - 1)

    def test_keys_are_still_mandatory_and_gates_still_need_them(self) -> None:
        maze = Maze.from_text(GATE_ON_THE_ONLY_ROUTE)
        result = min_energy_path(maze, bombs=0)
        self.assert_valid_blast_walk(maze, result, bombs=0)
        energy, path = result
        self.assertEqual(energy, len(shortest_path_all_keys(maze)) - 1)
        self.assertLess(len(set(path)), len(path), "expected the route to revisit cells")

    def test_no_bombs_means_a_blocked_route_is_none(self) -> None:
        self.assertIsNone(min_energy_path(Maze.from_text(ONE_WALL), bombs=0))

    def test_one_bomb_opens_the_only_route(self) -> None:
        maze = Maze.from_text(ONE_WALL)
        result = min_energy_path(maze, bombs=1)
        self.assert_valid_blast_walk(maze, result, bombs=1)
        self.assertEqual(result[0], 1 + BOMB_COST + 1 + 1)

    def test_bomb_budget_is_enforced_across_the_whole_walk(self) -> None:
        self.assertIsNone(min_energy_path(Maze.from_text(TWO_WALLS), bombs=1))
        result = min_energy_path(Maze.from_text(TWO_WALLS), bombs=2)
        self.assert_valid_blast_walk(Maze.from_text(TWO_WALLS), result, bombs=2)
        self.assertEqual(result[0], 2 * BOMB_COST + 2)

    def test_blasts_when_the_detour_is_more_expensive(self) -> None:
        maze = Maze.from_text(BOMB_BEATS_DETOUR)
        self.assertEqual(min_energy_path(maze, bombs=0)[0], 12)
        result = min_energy_path(maze, bombs=1)
        self.assert_valid_blast_walk(maze, result, bombs=1)
        energy, path = result
        self.assertEqual(energy, 11)
        self.assertIn(Coord(1, 5), path, "expected the walk to go through the wall")

    def test_keeps_the_bomb_when_walking_around_is_cheaper(self) -> None:
        maze = Maze.from_text(DETOUR_BEATS_BOMB)
        result = min_energy_path(maze, bombs=1)
        self.assert_valid_blast_walk(maze, result, bombs=1)
        energy, path = result
        self.assertEqual(energy, 6)
        self.assertNotIn(Coord(1, 3), path, "the wall was cheaper to walk around")

    def test_the_outer_border_is_bedrock(self) -> None:
        """No budget lets the walk tunnel out through the frame and cut a corner."""
        maze = Maze.from_text(ONE_WALL)
        for col in range(maze.cols):
            self.assertFalse(maze.is_bombable(Coord(0, col)))
            self.assertFalse(maze.is_bombable(Coord(maze.rows - 1, col)))
        result = min_energy_path(maze, bombs=5)
        self.assert_valid_blast_walk(maze, result, bombs=5)
        energy, path = result
        self.assertEqual(energy, 1 + BOMB_COST + 1 + 1)
        for cell in path:
            self.assertTrue(
                0 < cell.row < maze.rows - 1 and 0 < cell.col < maze.cols - 1,
                f"walk stepped onto the frame at {cell}",
            )

    def test_negative_bomb_budget_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            min_energy_path(Maze.from_text(SQUARE), bombs=-1)

    def test_large_grid_with_keys_and_bombs_under_time_budget(self) -> None:
        """61x61 x 2^4 key sets x 2 bomb states — a heap keeps it comfortable."""
        maze = big_maze(61)
        start = time.perf_counter()
        result = min_energy_path(maze, bombs=1)
        elapsed = time.perf_counter() - start
        self.assert_valid_blast_walk(maze, result, bombs=1)
        self.assertEqual(result[0], len(shortest_path_all_keys(maze)) - 1)
        self.assertLess(elapsed, 5.0, f"Dijkstra took {elapsed:.2f}s on 61x61 with 4 keys")


if __name__ == "__main__":
    unittest.main(verbosity=2)
