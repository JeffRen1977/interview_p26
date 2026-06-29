// LeetCode 704 - Binary Search.

#include <cassert>
#include <iostream>
#include <vector>

int binarySearch(const std::vector<int>& nums, int target) {
    int left = 0, right = static_cast<int>(nums.size()) - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] == target) return mid;
        if (nums[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

int main() {
    assert(binarySearch({-1, 0, 3, 5, 9, 12}, 9) == 4);
    assert(binarySearch({-1, 0, 3, 5, 9, 12}, 2) == -1);
    std::cout << "binary_search: ok\n";
    return 0;
}
