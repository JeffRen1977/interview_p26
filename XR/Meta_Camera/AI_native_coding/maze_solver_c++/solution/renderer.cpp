#include "renderer.h"

// Phase 1 fix: index ROWS first, then columns.
//
// The buggy version wrote grid[coord.col][coord.row]. On a square maze that
// silently draws the transposed path; on a rectangular one it indexes past the
// end of the row string, which in C++ is undefined behaviour rather than the
// IndexError Python would have raised.
std::string render(const Maze& maze, const std::vector<Coord>& path) {
    std::vector<std::string> grid = maze.grid();
    for (const Coord& coord : path) {
        const auto r = static_cast<std::size_t>(coord.row);
        const auto c = static_cast<std::size_t>(coord.col);
        if (grid[r][c] == kOpen) grid[r][c] = kPath;
    }
    std::string out;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        if (i) out += '\n';
        out += grid[i];
    }
    return out;
}
