// LeetCode 973 - K Closest Points to Origin.

#include <cassert>
#include <iostream>
#include <queue>
#include <vector>

std::vector<std::vector<int>> kClosest(std::vector<std::vector<int>> points, int k) {
    using T = std::tuple<int, int, int>;  // (-dist, x, y)
    auto cmp = [](const T& a, const T& b) { return std::get<0>(a) > std::get<0>(b); };
    std::priority_queue<T, std::vector<T>, decltype(cmp)> heap(cmp);

    for (const auto& p : points) {
        int dist = p[0] * p[0] + p[1] * p[1];
        heap.push({-dist, p[0], p[1]});
        if (static_cast<int>(heap.size()) > k) heap.pop();
    }

    std::vector<std::vector<int>> result;
    while (!heap.empty()) {
        auto [_, x, y] = heap.top();
        heap.pop();
        result.push_back({x, y});
    }
    return result;
}

int main() {
    auto got = kClosest({{1, 3}, {-2, 2}}, 1);
    assert(got.size() == 1);
    assert(got[0] == std::vector<int>({-2, 2}));
    std::cout << "k_closest_points: ok\n";
    return 0;
}
