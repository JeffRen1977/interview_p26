#include "renderer.h"

std::string render(const Maze& maze, const std::vector<Coord>& path) {
    std::vector<std::string> grid = maze.grid();
    for (const Coord& coord : path) {
        const auto r = static_cast<std::size_t>(coord.col);
        const auto c = static_cast<std::size_t>(coord.row);
        // std::string::operator[] does not bounds-check, so guard before writing.
        if (r >= grid.size() || c >= grid[r].size()) continue;
        if (grid[r][c] == kOpen) grid[r][c] = kPath;
    }
    std::string out;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        if (i) out += '\n';
        out += grid[i];
    }
    return out;
}
