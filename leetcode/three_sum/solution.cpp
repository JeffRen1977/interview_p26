// LeetCode 15 - Three Sum. Sort + two pointers O(n^2).

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

std::vector<std::vector<int>> threeSum(std::vector<int> nums) {
    std::sort(nums.begin(), nums.end());
    const int n = static_cast<int>(nums.size());
    std::vector<std::vector<int>> result;

    for (int i = 0; i < n - 2; ++i) {
        if (i > 0 && nums[i] == nums[i - 1]) continue;
        int left = i + 1, right = n - 1;
        while (left < right) {
            const int total = nums[i] + nums[left] + nums[right];
            if (total == 0) {
                result.push_back({nums[i], nums[left], nums[right]});
                ++left;
                --right;
                while (left < right && nums[left] == nums[left - 1]) ++left;
                while (left < right && nums[right] == nums[right + 1]) --right;
            } else if (total < 0) {
                ++left;
            } else {
                --right;
            }
        }
    }
    return result;
}

int main() {
    auto got = threeSum({-1, 0, 1, 2, -1, -4});
    assert(got.size() == 2);
    std::cout << "three_sum: ok\n";
    return 0;
}
