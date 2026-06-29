"""LeetCode 347 - Top K Frequent Elements. Min-heap O(n log k)."""

from collections import Counter
from typing import List
import heapq


def top_k_frequent(nums: List[int], k: int) -> List[int]:
    counts = Counter(nums)
    heap: List[tuple[int, int]] = []
    for num, freq in counts.items():
        heapq.heappush(heap, (freq, num))
        if len(heap) > k:
            heapq.heappop(heap)
    return [num for _, num in heap]


if __name__ == "__main__":
    assert sorted(top_k_frequent([1, 1, 1, 2, 2, 3], 2)) == [1, 2]
    print("top_k_frequent: ok")
