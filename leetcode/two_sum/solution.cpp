// LeetCode 1 - Two Sum. Hash map O(n).

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

int main() {
    auto r1 = twoSum({2, 7, 11, 15}, 9);
    assert(r1.size() == 2);
    auto r2 = twoSum({3, 2, 4}, 6);
    assert(r2 == std::vector<int>({1, 2}));
    std::cout << "two_sum: ok\n";
    return 0;
}
