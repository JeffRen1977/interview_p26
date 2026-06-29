// LeetCode 200 - Number of Islands. DFS.

#include <cassert>
#include <iostream>
#include <vector>

void dfs(std::vector<std::vector<char>>& grid, int r, int c) {
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid[0].size());
    if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] != '1') return;
    grid[r][c] = '0';
    dfs(grid, r + 1, c);
    dfs(grid, r - 1, c);
    dfs(grid, r, c + 1);
    dfs(grid, r, c - 1);
}

int numIslands(std::vector<std::vector<char>> grid) {
    if (grid.empty() || grid[0].empty()) return 0;
    int count = 0;
    for (int r = 0; r < static_cast<int>(grid.size()); ++r) {
        for (int c = 0; c < static_cast<int>(grid[0].size()); ++c) {
            if (grid[r][c] == '1') {
                ++count;
                dfs(grid, r, c);
            }
        }
    }
    return count;
}

int main() {
    std::vector<std::vector<char>> grid = {
        {'1', '1', '0', '0', '0'},
        {'1', '1', '0', '0', '0'},
        {'0', '0', '1', '0', '0'},
        {'0', '0', '0', '1', '1'},
    };
    assert(numIslands(grid) == 3);
    std::cout << "number_of_islands: ok\n";
    return 0;
}
