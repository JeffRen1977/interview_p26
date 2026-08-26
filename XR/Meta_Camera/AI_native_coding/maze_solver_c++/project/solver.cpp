#include "solver.h"

#include <stdexcept>

namespace {

bool dfs(const Maze& maze, const Coord& start, const Coord& goal, int depth) {
    if (depth > kRecursionLimit) {
        throw std::runtime_error("recursion limit exceeded in dfs_reachable");
    }
    if (start == goal) return true;
    for (const Coord& next : maze.neighbors(start)) {
        if (dfs(maze, next, goal, depth + 1)) return true;
    }
    return false;
}

}  // namespace

bool dfs_reachable(const Maze& maze, const Coord& start, const Coord& goal) {
    return dfs(maze, start, goal, 0);
}

// ---------------------------------------------------------------------------
// Phase 2
// ---------------------------------------------------------------------------
std::optional<std::vector<Coord>> shortest_path(const Maze& maze) {
    (void)maze;
    // TODO(Phase 2): BFS with a visited set and a parent map for reconstruction.
    throw std::logic_error("Phase 2: implement shortest_path");
}

// ---------------------------------------------------------------------------
// Phase 3
// ---------------------------------------------------------------------------
std::optional<std::vector<Coord>> shortest_path_all_keys(const Maze& maze) {
    (void)maze;
    // TODO(Phase 3): BFS over (row, col, keys_mask). Visited must be keyed by
    // the full state, not by the cell, or you will prune the only legal route.
    throw std::logic_error("Phase 3: implement shortest_path_all_keys");
}

// ---------------------------------------------------------------------------
// Phase 4
// ---------------------------------------------------------------------------
std::optional<std::pair<int, std::vector<Coord>>> min_energy_path(const Maze& maze, int bombs) {
    (void)maze;
    (void)bombs;
    // TODO(Phase 4): edge weights are no longer uniform, so BFS is out. Dijkstra
    // over (cell, keys_mask, bombs_left), and the goal check moves to pop time.
    throw std::logic_error("Phase 4: implement min_energy_path");
}
