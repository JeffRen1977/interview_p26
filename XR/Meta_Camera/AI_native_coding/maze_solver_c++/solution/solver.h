// Path finding. Reference implementation.
#pragma once

#include "grid.h"
#include "maze.h"

#include <optional>
#include <utility>
#include <vector>

// C++ has no recursion limit of its own: an unguarded DFS on a cyclic grid
// overflows the stack and takes the whole test binary down with it, with no
// exception to catch. This explicit limit stands in for Python's
// RecursionError so a runaway search is reportable instead of fatal.
inline constexpr int kRecursionLimit = 4000;

// Can we walk from start to goal ignoring keys and gates?
// Throws std::runtime_error if the search exceeds kRecursionLimit.
bool dfs_reachable(const Maze& maze, const Coord& start, const Coord& goal);

// Fewest-cells path from maze.start to maze.end, ignoring keys/gates.
// Includes both endpoints; std::nullopt if unreachable.
// A maze whose start equals its end returns {start}.
std::optional<std::vector<Coord>> shortest_path(const Maze& maze);

// Shortest walk that collects EVERY key and then stands on maze.end.
// Gates 'A'..'D' are walls until the matching key 'a'..'d' is collected.
// Cells may be revisited -- with a different key set they are different states.
std::optional<std::vector<Coord>> shortest_path_all_keys(const Maze& maze);

// Cheapest walk that collects every key and ends on maze.end, with bombs.
//
// Energy model (see grid.h):
//   - stepping into an open cell costs maze.terrain_cost(cell): '.'/'S'/'E'/
//     keys/gates cost 1, rough terrain '~' costs 5
//   - stepping into an *interior* wall costs kBombCost and burns one bomb;
//     `bombs` is the budget for the whole walk. The outer border is bedrock
//     (maze.is_bombable says so) and can never be blown open
//   - the starting cell is free -- cost is charged per step taken
//   - gates still need their key, exactly as in Phase 3
//
// Returns (total_energy, path) including both endpoints, or nullopt if there is
// no walk that collects every key within the bomb budget.
// bombs < 0 throws std::invalid_argument.
std::optional<std::pair<int, std::vector<Coord>>> min_energy_path(const Maze& maze, int bombs = 0);
