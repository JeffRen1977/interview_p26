// Maze model: walls, start/end, keys and gates. Given -- do not change this file.
#pragma once

#include "grid.h"

#include <map>
#include <string>
#include <vector>

// A rectangular grid.
//
// Legend: '#' wall, '.' open, 'S' start, 'E' end, '~' rough terrain,
//         'a'..'d' key, 'A'..'D' the gate that key opens.
class Maze {
public:
    explicit Maze(std::vector<std::string> grid);
    static Maze from_text(const std::string& text);

    char at(const Coord& c) const { return grid_[static_cast<std::size_t>(c.row)][static_cast<std::size_t>(c.col)]; }
    bool in_bounds(const Coord& c) const {
        return c.row >= 0 && c.row < rows_ && c.col >= 0 && c.col < cols_;
    }
    bool is_wall(const Coord& c) const { return at(c) == kWall; }

    // Passable given the keys collected so far. A gate is a wall until its key
    // is in the mask.
    bool is_open(const Coord& c, int keys_mask = 0) const;

    // Passable 4-neighbours, in kDirections order.
    std::vector<Coord> neighbors(const Coord& c, int keys_mask = 0) const;

    // Every in-bounds 4-neighbour, WALLS INCLUDED, in kDirections order.
    // Phase 4 needs to *see* walls in order to decide whether to bomb one.
    std::vector<Coord> raw_neighbors(const Coord& c) const;

    // Energy to step into `c`. Says nothing about whether it is passable.
    int terrain_cost(const Coord& c) const { return at(c) == kRough ? kRoughCost : kDefaultCost; }

    // True iff `c` is an *interior* wall. The outer border is bedrock.
    bool is_bombable(const Coord& c) const;

    int all_keys_mask() const;

    const std::vector<std::string>& grid() const { return grid_; }
    int rows() const { return rows_; }
    int cols() const { return cols_; }

    Coord start;
    Coord end;
    std::map<char, Coord> keys;
    std::map<char, Coord> gates;

private:
    std::vector<std::string> grid_;
    int rows_ = 0;
    int cols_ = 0;
};
