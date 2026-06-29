// LeetCode 1 - Two Sum. Hash map O(n).
// LeetCode 167 variant - Two Sum II: two pointers when nums is sorted.

#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

std::vector<int> twoSum(const std::vector<int>& nums, int target) {
    std::unordered_map<int, int> seen;
    for (int i = 0; i < static_cast<int>(nums.size()); ++i) {
        int need = target - nums[i];
        auto it = seen.find(need);
        if (it != seen.end()) {
            return {it->second, i};
        }
        seen[nums[i]] = i;
    }
    return {};
}

std::vector<int> twoSumSorted(const std::vector<int>& nums, int target) {
    int left = 0, right = static_cast<int>(nums.size()) - 1;
    while (left < right) {
        int sum = nums[left] + nums[right];
        if (sum == target) {
            return {left, right};
        }
        if (sum < target) {
            ++left;
        } else {
            --right;
        }
    }
    return {};
}

int main() {
    auto r1 = twoSum({2, 7, 11, 15}, 9);
    assert(r1.size() == 2);
    auto r2 = twoSum({3, 2, 4}, 6);
    assert(r2 == std::vector<int>({1, 2}));

    assert(twoSumSorted({2, 7, 11, 15}, 9) == std::vector<int>({0, 1}));
    assert(twoSumSorted({2, 3, 4}, 6) == std::vector<int>({0, 2}));
    assert(twoSumSorted({1, 2, 3, 4, 5}, 9) == std::vector<int>({3, 4}));
    std::cout << "two_sum: ok\n";
    return 0;
}
