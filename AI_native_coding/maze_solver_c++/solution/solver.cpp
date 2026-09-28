#include "solver.h"

#include <algorithm>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace {

// Phase 1 fix: without `visited`, two adjacent open cells recurse forever.
// The stack overflow was never about maze size -- a 3-cell corridor is enough.
bool dfs(const Maze& maze, const Coord& start, const Coord& goal, std::set<Coord>& visited,
         int depth) {
    if (depth > kRecursionLimit) {
        throw std::runtime_error("recursion limit exceeded in dfs_reachable");
    }
    if (start == goal) return true;
    visited.insert(start);
    for (const Coord& next : maze.neighbors(start)) {
        if (visited.count(next) == 0 && dfs(maze, next, goal, visited, depth + 1)) return true;
    }
    return false;
}

int pickup(const Maze& maze, const Coord& c, int mask) {
    const char ch = maze.at(c);
    return is_key(ch) ? (mask | (1 << key_bit(ch))) : mask;
}

}  // namespace

bool dfs_reachable(const Maze& maze, const Coord& start, const Coord& goal) {
    std::set<Coord> visited;
    return dfs(maze, start, goal, visited, 0);
}

// ---------------------------------------------------------------------------
// Phase 2 -- BFS
// ---------------------------------------------------------------------------
// On an unweighted grid the first time BFS reaches the goal it is optimal;
// DFS would return *a* path, which is not the same thing.
std::optional<std::vector<Coord>> shortest_path(const Maze& maze) {
    const Coord start = maze.start;
    const Coord end = maze.end;
    if (start == end) return std::vector<Coord>{start};

    std::map<Coord, Coord> parent;   // child -> parent
    std::set<Coord> seen{start};
    std::deque<Coord> queue{start};

    auto rebuild = [&](Coord node) {
        std::vector<Coord> path;
        while (true) {
            path.push_back(node);
            if (node == start) break;
            node = parent.at(node);
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    while (!queue.empty()) {
        const Coord current = queue.front();
        queue.pop_front();
        for (const Coord& next : maze.neighbors(current)) {
            if (seen.count(next)) continue;   // seen doubles as visited
            seen.insert(next);
            parent[next] = current;
            if (next == end) return rebuild(next);
            queue.push_back(next);
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Phase 3 -- bitmask BFS over (cell, keys_mask)
// ---------------------------------------------------------------------------
// Why the naive approach fails: "BFS to the nearest key, then the next" is a
// greedy ordering and is not optimal, and enumerating all k! key orders with
// pairwise BFS blows up. Folding the key set into the state keeps one BFS,
// O(rows * cols * 2^k) states, and it is still shortest-path-optimal because
// every edge costs 1.
//
// Visited MUST be keyed by (cell, mask). Keying it by cell alone prunes the
// revisit that carries a new key, which is exactly how the only legal route
// disappears.
namespace {
using KeyState = std::pair<Coord, int>;
}

std::optional<std::vector<Coord>> shortest_path_all_keys(const Maze& maze) {
    const Coord start = maze.start;
    const Coord end = maze.end;
    const int goal_mask = maze.all_keys_mask();
    const int start_mask = pickup(maze, start, 0);

    if (start == end && start_mask == goal_mask) return std::vector<Coord>{start};

    const KeyState origin{start, start_mask};
    std::map<KeyState, KeyState> parent;
    std::set<KeyState> seen{origin};
    std::deque<KeyState> queue{origin};

    auto rebuild = [&](KeyState node) {
        std::vector<Coord> path;
        while (true) {
            path.push_back(node.first);
            if (node == origin) break;
            node = parent.at(node);
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    while (!queue.empty()) {
        const KeyState state = queue.front();
        queue.pop_front();
        const auto& [coord, mask] = state;
        for (const Coord& next : maze.neighbors(coord, mask)) {
            const KeyState next_state{next, pickup(maze, next, mask)};
            if (seen.count(next_state)) continue;
            seen.insert(next_state);
            parent[next_state] = state;
            if (next == end && next_state.second == goal_mask) return rebuild(next_state);
            queue.push_back(next_state);
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Phase 4 -- Dijkstra over (cell, keys_mask, bombs_left)
// ---------------------------------------------------------------------------
// Why BFS stops working: a step onto rough terrain costs 5 and a blast costs
// kBombCost, so the queue no longer visits states in cost order. The fix is a
// priority queue, and with it two rules change:
//
//   1. the goal test moves from push time to *pop* time -- a state is only
//      settled once it comes off the heap;
//   2. `visited` becomes `dist`, a best-cost-so-far table. A state is worth
//      re-expanding whenever we reach it more cheaply than before.
//
// Bombs are a budget, not a map edit: a blast is charged the moment we step
// into the wall, so nothing about the grid changes and `bombs_left` is all the
// extra state we need. State space is rows * cols * 2^k * (bombs + 1).
namespace {
struct EnergyState {
    Coord coord;
    int mask = 0;
    int bombs_left = 0;

    bool operator<(const EnergyState& o) const {
        if (coord != o.coord) return coord < o.coord;
        if (mask != o.mask) return mask < o.mask;
        return bombs_left < o.bombs_left;
    }
    bool operator==(const EnergyState& o) const {
        return coord == o.coord && mask == o.mask && bombs_left == o.bombs_left;
    }
};

struct HeapEntry {
    int cost;
    EnergyState state;
    // std::priority_queue is a MAX-heap, so invert the comparison to pop the
    // cheapest first. Tie-break on the state to keep the order deterministic.
    bool operator<(const HeapEntry& o) const {
        return cost != o.cost ? cost > o.cost : o.state < state;
    }
};
}  // namespace

std::optional<std::pair<int, std::vector<Coord>>> min_energy_path(const Maze& maze, int bombs) {
    if (bombs < 0) throw std::invalid_argument("bombs must be >= 0");

    const Coord start = maze.start;
    const Coord end = maze.end;
    const int goal_mask = maze.all_keys_mask();
    const EnergyState origin{start, pickup(maze, start, 0), bombs};

    std::map<EnergyState, int> dist{{origin, 0}};
    std::map<EnergyState, EnergyState> parent;
    std::priority_queue<HeapEntry> heap;
    heap.push(HeapEntry{0, origin});

    auto rebuild = [&](EnergyState node) {
        std::vector<Coord> path;
        while (true) {
            path.push_back(node.coord);
            if (node == origin) break;
            node = parent.at(node);
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    while (!heap.empty()) {
        const HeapEntry entry = heap.top();
        heap.pop();
        const auto it = dist.find(entry.state);
        if (it == dist.end() || entry.cost > it->second) continue;  // stale heap entry

        const EnergyState& state = entry.state;
        if (state.coord == end && state.mask == goal_mask) {
            return std::make_pair(entry.cost, rebuild(state));
        }

        for (const Coord& next : maze.raw_neighbors(state.coord)) {
            int step = 0;
            int next_bombs = state.bombs_left;
            if (maze.is_wall(next)) {
                if (state.bombs_left == 0 || !maze.is_bombable(next)) continue;
                step = kBombCost;
                next_bombs = state.bombs_left - 1;
            } else if (!maze.is_open(next, state.mask)) {
                continue;  // a gate we have no key for
            } else {
                step = maze.terrain_cost(next);
            }

            const EnergyState next_state{next, pickup(maze, next, state.mask), next_bombs};
            const int next_cost = entry.cost + step;
            const auto found = dist.find(next_state);
            if (found == dist.end() || next_cost < found->second) {
                dist[next_state] = next_cost;
                parent[next_state] = state;
                heap.push(HeapEntry{next_cost, next_state});
            }
        }
    }
    return std::nullopt;
}
