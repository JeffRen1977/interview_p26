#include "MazeSolver.hpp"
#include <iostream>
#include <string>

int main() {
    Maze maze;
    std::string row;

    while (std::getline(std::cin, row)) {
        if (!row.empty()) {
            maze.push_back(row);
        }
    }

    try {
        MazeSolver solver;
        const std::optional<MazeSolution> solution = solver.solve(maze);

        if (!solution.has_value()) {
            std::cout << "No path\n";
            return 1;
        }

        const Maze renderedMaze = solver.renderPath(maze, *solution);

        for (const std::string& renderedRow : renderedMaze) {
            std::cout << renderedRow << '\n';
        }

        std::cout << "Path:\n";

        for (const Position& position : solution->path) {
            std::cout
                << '(' << position.first
                << ", " << position.second
                << ")\n";
        }
    } catch (const std::invalid_argument& error) {
        std::cerr << "Invalid maze: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
