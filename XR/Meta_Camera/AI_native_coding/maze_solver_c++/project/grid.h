// Grid parsing and coordinates. Given -- do not change this file.
#pragma once

#include <array>
#include <string>
#include <vector>

inline constexpr char kWall = '#';
inline constexpr char kOpen = '.';
inline constexpr char kStart = 'S';
inline constexpr char kEnd = 'E';
inline constexpr char kPath = '*';
//: Rough terrain: passable, but a step into it costs more than a step onto '.'.
inline constexpr char kRough = '~';

//: Gates stop at D on purpose: 'E' is already the exit marker.
inline constexpr const char* kKeys = "abcd";
inline constexpr const char* kGates = "ABCD";

//: Energy to step INTO a cell. Anything not '~' costs kDefaultCost.
inline constexpr int kDefaultCost = 1;
inline constexpr int kRoughCost = 5;
//: Energy to blow through one wall. Also consumes one bomb from the budget.
inline constexpr int kBombCost = 4;

// (row, col) -- row first, always. Mixing the two up is the classic bug.
struct Coord {
    int row = 0;
    int col = 0;

    Coord() = default;
    Coord(int r, int c) : row(r), col(c) {}

    Coord shifted(int dr, int dc) const { return Coord(row + dr, col + dc); }

    bool operator==(const Coord& o) const { return row == o.row && col == o.col; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    bool operator<(const Coord& o) const { return row != o.row ? row < o.row : col < o.col; }

    std::string str() const { return "(" + std::to_string(row) + "," + std::to_string(col) + ")"; }
};

//: Neighbour order is part of the contract: up, down, left, right.
inline constexpr std::array<std::array<int, 2>, 4> kDirections = {{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};

// Parse a maze drawing into a rectangular grid of single characters.
// Blank lines are ignored. Every row must have the same width.
// Throws std::invalid_argument otherwise.
std::vector<std::string> parse_grid(const std::string& text);

// Bit index for a key/gate letter: a/A -> 0, b/B -> 1, ...
int key_bit(char ch);

bool is_key(char ch);
bool is_gate(char ch);
