"""LeetCode 46 - Permutations. Backtracking O(n * n!)."""

from typing import List


def permute(nums: List[int]) -> List[List[int]]:
    result: List[List[int]] = []
    path: List[int] = []
    used = [False] * len(nums)

    def backtrack() -> None:
        if len(path) == len(nums):
            result.append(path.copy())
            return
        for i in range(len(nums)):
            if used[i]:
                continue
            used[i] = True
            path.append(nums[i])
            backtrack()
            path.pop()
            used[i] = False

    backtrack()
    return result


if __name__ == "__main__":
    perms = permute([1, 2, 3])
    assert len(perms) == 6
    print("permutations: ok")
