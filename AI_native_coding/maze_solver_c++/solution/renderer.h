// Maze rendering. Reference implementation.
#pragma once

#include "grid.h"
#include "maze.h"

#include <string>
#include <vector>

// Draw the maze, overlaying '*' on every OPEN cell the path walks through.
// S, E, keys and gates keep their own character -- the path must not hide them.
std::string render(const Maze& maze, const std::vector<Coord>& path = {});
