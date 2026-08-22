"""LeetCode 76 - Minimum Window Substring.
Sliding window: expand right, shrink left while t is covered.
"""

from collections import Counter
from typing import Dict


def is_valid(pattern: Dict[str, int], target: Dict[str, int]) -> bool:
    for ch, need in pattern.items():
        if target.get(ch, 0) < need:
            return False
    return True


def min_window(s: str, t: str) -> str:
    pattern = Counter(t)
    left = 0
    target: Dict[str, int] = {}
    res = ""

    for right, ch in enumerate(s):
        target[ch] = target.get(ch, 0) + 1
        while is_valid(pattern, target):
            length = right - left + 1
            if not res or length < len(res):
                res = s[left : right + 1]
            target[s[left]] -= 1
            left += 1
    return res


if __name__ == "__main__":
    assert min_window("ADOBECODEBANC", "ABC") == "BANC"
    assert min_window("a", "a") == "a"
    assert min_window("a", "aa") == ""
    assert min_window("ab", "b") == "b"
    print("minimum_window_substring: ok")
