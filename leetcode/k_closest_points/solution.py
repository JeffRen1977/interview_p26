"""LeetCode 973 - K Closest Points to Origin. Max-heap O(n log k)."""

from typing import List
import heapq


def k_closest(points: List[List[int]], k: int) -> List[List[int]]:
    heap: List[tuple[int, int, int]] = []
    for x, y in points:
        dist = x * x + y * y
        heapq.heappush(heap, (-dist, x, y))
        if len(heap) > k:
            heapq.heappop(heap)
    return [[x, y] for _, x, y in heap]


if __name__ == "__main__":
    got = k_closest([[1, 3], [-2, 2]], 1)
    assert got == [[-2, 2]]
    print("k_closest_points: ok")
