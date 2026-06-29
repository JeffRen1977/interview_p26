// LeetCode 347 - Top K Frequent Elements. Min-heap.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>

std::vector<int> topKFrequent(const std::vector<int>& nums, int k) {
    std::unordered_map<int, int> counts;
    for (int x : nums) ++counts[x];

    using Pair = std::pair<int, int>;  // (freq, num)
    auto cmp = [](const Pair& a, const Pair& b) { return a.first > b.first; };
    std::priority_queue<Pair, std::vector<Pair>, decltype(cmp)> heap(cmp);

    for (const auto& [num, freq] : counts) {
        heap.push({freq, num});
        if (static_cast<int>(heap.size()) > k) heap.pop();
    }

    std::vector<int> result;
    while (!heap.empty()) {
        result.push_back(heap.top().second);
        heap.pop();
    }
    return result;
}

int main() {
    auto got = topKFrequent({1, 1, 1, 2, 2, 3}, 2);
    std::sort(got.begin(), got.end());
    assert(got == std::vector<int>({1, 2}));
    std::cout << "top_k_frequent: ok\n";
    return 0;
}
