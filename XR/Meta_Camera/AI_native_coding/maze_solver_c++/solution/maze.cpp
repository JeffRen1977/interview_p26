#include "maze.h"

#include <stdexcept>

Maze::Maze(std::vector<std::string> grid) : grid_(std::move(grid)) {
    rows_ = static_cast<int>(grid_.size());
    cols_ = rows_ > 0 ? static_cast<int>(grid_.front().size()) : 0;

    bool saw_start = false;
    bool saw_end = false;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            const Coord here(r, c);
            const char ch = at(here);
            if (ch == kStart) {
                start = here;
                saw_start = true;
            } else if (ch == kEnd) {
                end = here;
                saw_end = true;
            } else if (is_key(ch)) {
                keys[ch] = here;
            } else if (is_gate(ch)) {
                gates[ch] = here;
            }
        }
    }
    if (!saw_start || !saw_end) {
        throw std::invalid_argument("maze needs both a start (S) and an end (E)");
    }
}

Maze Maze::from_text(const std::string& text) { return Maze(parse_grid(text)); }

bool Maze::is_open(const Coord& c, int keys_mask) const {
    if (!in_bounds(c) || is_wall(c)) return false;
    const char ch = at(c);
    if (is_gate(ch)) return (keys_mask & (1 << key_bit(ch))) != 0;
    return true;
}

std::vector<Coord> Maze::neighbors(const Coord& c, int keys_mask) const {
    std::vector<Coord> out;
    for (const auto& [dr, dc] : kDirections) {
        const Coord next = c.shifted(dr, dc);
        if (is_open(next, keys_mask)) out.push_back(next);
    }
    return out;
}

std::vector<Coord> Maze::raw_neighbors(const Coord& c) const {
    std::vector<Coord> out;
    for (const auto& [dr, dc] : kDirections) {
        const Coord next = c.shifted(dr, dc);
        if (in_bounds(next)) out.push_back(next);
    }
    return out;
}

bool Maze::is_bombable(const Coord& c) const {
    if (!in_bounds(c) || !is_wall(c)) return false;
    return c.row > 0 && c.row < rows_ - 1 && c.col > 0 && c.col < cols_ - 1;
}

int Maze::all_keys_mask() const {
    int mask = 0;
    for (const auto& [ch, _] : keys) mask |= 1 << key_bit(ch);
    return mask;
}
