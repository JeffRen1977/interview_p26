#include "MazeSolver.hpp"

#include <array>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace {

struct SearchState {
    int row;
    int column;
    int collectedKeysMask;

    bool operator==(const SearchState& other) const {
        return row == other.row
            && column == other.column
            && collectedKeysMask == other.collectedKeysMask;
    }
};

struct SearchStateHash {
    std::size_t operator()(const SearchState& state) const {
        std::size_t hash = static_cast<std::size_t>(state.row);
        hash = hash * 31 + static_cast<std::size_t>(state.column);
        hash = hash * 31 + static_cast<std::size_t>(state.collectedKeysMask);
        return hash;
    }
};

constexpr std::array<Position, 4> directions = {
    Position{-1, 0},
    Position{1, 0},
    Position{0, -1},
    Position{0, 1}
};

bool isKey(char cell) {
    return cell >= 'a' && cell <= 'd';
}

bool isDoor(char cell) {
    return cell >= 'A' && cell <= 'D';
}

int getKeyBit(char key) {
    return 1 << (key - 'a');
}

int getDoorKeyBit(char door) {
    return 1 << (door - 'A');
}

bool isSupportedCell(char cell) {
    return cell == '#'
        || cell == '.'
        || cell == 'S'
        || cell == 'E'
        || isKey(cell)
        || isDoor(cell);
}

Position validateAndFindStart(const Maze& maze) {
    if (maze.empty() || maze.front().empty()) {
        throw std::invalid_argument("Maze must not be empty");
    }

    const std::size_t expectedWidth = maze.front().size();
    std::optional<Position> start;
    int endingCount = 0;

    for (std::size_t row = 0; row < maze.size(); ++row) {
        if (maze[row].size() != expectedWidth) {
            throw std::invalid_argument("Maze must be rectangular");
        }

        for (std::size_t column = 0; column < maze[row].size(); ++column) {
            const char cell = maze[row][column];

            if (!isSupportedCell(cell)) {
                throw std::invalid_argument("Maze contains an unsupported cell");
            }

            if (cell == 'S') {
                if (start.has_value()) {
                    throw std::invalid_argument(
                        "Maze must contain exactly one start"
                    );
                }

                start = Position{
                    static_cast<int>(row),
                    static_cast<int>(column)
                };
            }

            if (cell == 'E') {
                ++endingCount;
            }
        }
    }

    if (!start.has_value()) {
        throw std::invalid_argument("Maze must contain exactly one start");
    }

    if (endingCount != 1) {
        throw std::invalid_argument("Maze must contain exactly one end");
    }

    return *start;
}

std::vector<Position> reconstructPath(
    const SearchState& endingState,
    const SearchState& startingState,
    const std::unordered_map<
        SearchState,
        SearchState,
        SearchStateHash
    >& parentByState
) {
    std::vector<Position> reversedPath;
    SearchState currentState = endingState;

    while (!(currentState == startingState)) {
        reversedPath.emplace_back(
            currentState.row,
            currentState.column
        );
        currentState = parentByState.at(currentState);
    }

    reversedPath.emplace_back(
        startingState.row,
        startingState.column
    );

    return std::vector<Position>(
        reversedPath.rbegin(),
        reversedPath.rend()
    );
}

}  // namespace

std::optional<MazeSolution> MazeSolver::solve(const Maze& maze) const {
    const Position startingPosition = validateAndFindStart(maze);

    SearchState startingState{
        startingPosition.first,
        startingPosition.second,
        0
    };

    std::queue<SearchState> pendingStates;
    std::unordered_map<SearchState, SearchState, SearchStateHash> parentByState;
    std::unordered_map<SearchState, bool, SearchStateHash> visitedStates;

    pendingStates.push(startingState);
    visitedStates.emplace(startingState, true);

    while (!pendingStates.empty()) {
        const SearchState currentState = pendingStates.front();
        pendingStates.pop();

        if (maze[currentState.row][currentState.column] == 'E') {
            return MazeSolution{
                reconstructPath(
                    currentState,
                    startingState,
                    parentByState
                )
            };
        }

        for (const Position& direction : directions) {
            const int nextRow = currentState.row + direction.first;
            const int nextColumn = currentState.column + direction.second;

            const bool isOutsideMaze =
                nextRow < 0
                || nextColumn < 0
                || nextRow >= static_cast<int>(maze.size())
                || nextColumn >= static_cast<int>(maze.front().size());

            if (isOutsideMaze) {
                continue;
            }

            const char nextCell = maze[nextRow][nextColumn];

            if (nextCell == '#') {
                continue;
            }

            if (isDoor(nextCell)) {
                const int requiredKeyBit = getDoorKeyBit(nextCell);

                if ((currentState.collectedKeysMask & requiredKeyBit) == 0) {
                    continue;
                }
            }

            int nextKeysMask = currentState.collectedKeysMask;

            if (isKey(nextCell)) {
                nextKeysMask |= getKeyBit(nextCell);
            }

            SearchState nextState{
                nextRow,
                nextColumn,
                nextKeysMask
            };

            if (visitedStates.find(nextState)!=visitedStates.end()) {
                continue;
            }

            visitedStates.emplace(nextState, true);
            parentByState.emplace(nextState, currentState);
            pendingStates.push(nextState);
        }
    }

    return std::nullopt;
}

Maze MazeSolver::renderPath(
    const Maze& maze,
    const MazeSolution& solution,
    char pathMarker
) const {
    Maze renderedMaze = maze;

    for (const Position& position : solution.path) {
        char& cell = renderedMaze[position.first][position.second];

        if (cell == '.') {
            cell = pathMarker;
        }
    }

    return renderedMaze;
}
