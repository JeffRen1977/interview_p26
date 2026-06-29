// LeetCode 56 - Merge Intervals.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

std::vector<std::vector<int>> mergeIntervals(std::vector<std::vector<int>> intervals) {
    if (intervals.empty()) return {};
    std::sort(intervals.begin(), intervals.end(),
              [](const auto& a, const auto& b) { return a[0] < b[0]; });
    std::vector<std::vector<int>> merged{intervals[0]};
    for (size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i][0] <= merged.back()[1]) {
            merged.back()[1] = std::max(merged.back()[1], intervals[i][1]);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

int main() {
    auto got = mergeIntervals({{1, 3}, {2, 6}, {8, 10}, {15, 18}});
    assert(got == std::vector<std::vector<int>>({{1, 6}, {8, 10}, {15, 18}}));
    std::cout << "merge_intervals: ok\n";
    return 0;
}
