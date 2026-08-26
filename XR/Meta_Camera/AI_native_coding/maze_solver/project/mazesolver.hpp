#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

using Maze = std::vector<std::string>;
using Position = std::pair<int, int>;

struct MazeSolution {
    std::vector<Position> path;
};

class MazeSolver {
public:
    std::optional<MazeSolution> solve(const Maze& maze) const;

    Maze renderPath(
        const Maze& maze,
        const MazeSolution& solution,
        char pathMarker = '*'
    ) const;
};
