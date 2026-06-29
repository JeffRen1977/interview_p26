"""LeetCode 1 - Two Sum. Hash map O(n).
LeetCode 167 variant - Two Sum II: two pointers when nums is sorted.
"""

from typing import Dict, List


def two_sum(nums: List[int], target: int) -> List[int]:
    """Unsorted array: hash map. Time O(n), Space O(n)."""
    seen: Dict[int, int] = {}
    for i, num in enumerate(nums):
        need = target - num
        if need in seen:
            return [seen[need], i]
        seen[num] = i
    return []


def two_sum_sorted(nums: List[int], target: int) -> List[int]:
    """Sorted array: two pointers. Time O(n), Space O(1)."""
    left, right = 0, len(nums) - 1
    while left < right:
        s = nums[left] + nums[right]
        if s == target:
            return [left, right]
        if s < target:
            left += 1
        else:
            right -= 1
    return []


if __name__ == "__main__":
    assert sorted(two_sum([2, 7, 11, 15], 9)) == [0, 1]
    assert sorted(two_sum([3, 2, 4], 6)) == [1, 2]

    assert two_sum_sorted([2, 7, 11, 15], 9) == [0, 1]
    assert two_sum_sorted([2, 3, 4], 6) == [0, 2]
    assert two_sum_sorted([1, 2, 3, 4, 5], 9) == [3, 4]
    print("two_sum: ok")
