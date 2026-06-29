"""
LeetCode Python 核心代码复习（无测试）
打印建议: python3 python_core_review.py 或直接从编辑器导出 PDF

题目索引:
  1.  Two Sum (哈希 / 排序双指针)
  2.  Three Sum
  3.  Merge Intervals
  4.  Binary Search
  5.  Search Rotated Array
  6.  Reverse Linked List
  7.  Merge Two Sorted Lists
  8.  LRU Cache (双向链表 O(1) / deque 简版 O(n))
  9.  Number of Islands
  10. Lowest Common Ancestor
  11. Top K Frequent
  12. K Closest Points
  13. Longest Substring Without Repeating
  14. Permutations
  15. Binary Tree Level Order
  16. SPSC Ring Buffer (工程题：单生产者单消费者)
"""

from __future__ import annotations

from collections import Counter, deque
from typing import Dict, Generic, List, Optional, TypeVar
import heapq

T = TypeVar("T")


# =============================================================================
# 1. Two Sum — 哈希 O(n) | 已排序双指针 O(n) O(1)空间
# =============================================================================

def two_sum(nums: List[int], target: int) -> List[int]:
    seen: Dict[int, int] = {}
    for i, num in enumerate(nums):
        need = target - num
        if need in seen:
            return [seen[need], i]
        seen[num] = i
    return []


def two_sum_sorted(nums: List[int], target: int) -> List[int]:
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


# =============================================================================
# 2. Three Sum — 排序 + 双指针 O(n^2)
# =============================================================================

def three_sum(nums: List[int]) -> List[List[int]]:
    nums.sort()
    n = len(nums)
    result: List[List[int]] = []

    for i in range(n - 2):
        if i > 0 and nums[i] == nums[i - 1]:
            continue
        left, right = i + 1, n - 1
        while left < right:
            total = nums[i] + nums[left] + nums[right]
            if total == 0:
                result.append([nums[i], nums[left], nums[right]])
                left += 1
                right -= 1
                while left < right and nums[left] == nums[left - 1]:
                    left += 1
                while left < right and nums[right] == nums[right + 1]:
                    right -= 1
            elif total < 0:
                left += 1
            else:
                right -= 1

    return result


# =============================================================================
# 3. Merge Intervals — 按起点排序 O(n log n)
# =============================================================================

def merge_intervals(intervals: List[List[int]]) -> List[List[int]]:
    if not intervals:
        return []
    intervals.sort(key=lambda x: x[0])
    merged = [intervals[0]]
    for start, end in intervals[1:]:
        if start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return merged


# =============================================================================
# 4. Binary Search — O(log n)
# =============================================================================

def binary_search(nums: List[int], target: int) -> int:
    left, right = 0, len(nums) - 1
    while left <= right:
        mid = left + (right - left) // 2
        if nums[mid] == target:
            return mid
        if nums[mid] < target:
            left = mid + 1
        else:
            right = mid - 1
    return -1


# =============================================================================
# 5. Search Rotated Array — 变形二分 O(log n)
# =============================================================================

def search_rotated(nums: List[int], target: int) -> int:
    left, right = 0, len(nums) - 1
    while left <= right:
        mid = left + (right - left) // 2
        if nums[mid] == target:
            return mid
        if nums[left] <= nums[mid]:
            if nums[left] <= target < nums[mid]:
                right = mid - 1
            else:
                left = mid + 1
        else:
            if nums[mid] < target <= nums[right]:
                left = mid + 1
            else:
                right = mid - 1
    return -1


# =============================================================================
# 6. Reverse Linked List — 迭代三指针 O(n)
# =============================================================================

class ListNode:
    def __init__(self, val: int = 0, next: Optional["ListNode"] = None):
        self.val = val
        self.next = next


def reverse_list(head: Optional[ListNode]) -> Optional[ListNode]:
    prev = None
    cur = head
    while cur:
        nxt = cur.next
        cur.next = prev
        prev = cur
        cur = nxt
    return prev


# =============================================================================
# 7. Merge Two Sorted Lists — dummy head O(n+m)
# =============================================================================

def merge_two_lists(
    l1: Optional[ListNode], l2: Optional[ListNode]
) -> Optional[ListNode]:
    dummy = ListNode()
    tail = dummy
    while l1 and l2:
        if l1.val <= l2.val:
            tail.next = l1
            l1 = l1.next
        else:
            tail.next = l2
            l2 = l2.next
        tail = tail.next
    tail.next = l1 if l1 else l2
    return dummy.next


# =============================================================================
# 8a. LRU Cache — 双向链表 + 哈希表 O(1) get/put（面试标准答案）
# =============================================================================

class Node:
    def __init__(self, key=0, value=0):
        self.key = key
        self.value = value
        self.prev = None
        self.next = None


class LRUCache:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.cache = {}

        self.head = Node()
        self.tail = Node()
        self.head.next = self.tail
        self.tail.prev = self.head

    def _remove(self, node):
        node.prev.next = node.next
        node.next.prev = node.prev

    def _add_to_front(self, node):
        node.next = self.head.next
        node.prev = self.head
        self.head.next.prev = node
        self.head.next = node

    def _move_to_front(self, node):
        self._remove(node)
        self._add_to_front(node)

    def _remove_lru(self):
        node = self.tail.prev
        self._remove(node)
        return node

    def get(self, key):
        if key not in self.cache:
            return -1
        node = self.cache[key]
        self._move_to_front(node)
        return node.value

    def put(self, key, value):
        if key in self.cache:
            node = self.cache[key]
            node.value = value
            self._move_to_front(node)
            return

        node = Node(key, value)
        self.cache[key] = node
        self._add_to_front(node)

        if len(self.cache) > self.capacity:
            lru = self._remove_lru()
            del self.cache[lru.key]


# =============================================================================
# 8b. LRU Cache — deque + dict 简版（好写；list.remove 最坏 O(n)）
# =============================================================================

class LRUCacheDeque:
    def __init__(self, capacity: int) -> None:
        self.capacity = capacity
        self.list = deque(maxlen=capacity)
        self.items: dict[int, int] = {}

    def get(self, key: int) -> int:
        if key not in self.items:
            return -1
        self.list.remove(key)
        self.list.append(key)
        return self.items[key]

    def put(self, key: int, value: int) -> None:
        if key in self.items:
            self.list.remove(key)
            self.list.append(key)
            self.items[key] = value
            return
        if len(self.items) == self.capacity:
            del self.items[self.list.popleft()]
        self.list.append(key)
        self.items[key] = value


# =============================================================================
# 9. Number of Islands — DFS 淹没法 O(mn)
# =============================================================================

def num_islands(grid: List[List[str]]) -> int:
    if not grid or not grid[0]:
        return 0
    rows, cols = len(grid), len(grid[0])
    count = 0

    def dfs(r: int, c: int) -> None:
        if r < 0 or r >= rows or c < 0 or c >= cols or grid[r][c] != "1":
            return
        grid[r][c] = "0"
        dfs(r + 1, c)
        dfs(r - 1, c)
        dfs(r, c + 1)
        dfs(r, c - 1)

    for r in range(rows):
        for c in range(cols):
            if grid[r][c] == "1":
                count += 1
                dfs(r, c)
    return count


# =============================================================================
# 10. Lowest Common Ancestor — 二叉树递归 / BST 性质
# =============================================================================

class TreeNode:
    def __init__(
        self,
        val: int = 0,
        left: Optional["TreeNode"] = None,
        right: Optional["TreeNode"] = None,
    ):
        self.val = val
        self.left = left
        self.right = right


def lowest_common_ancestor(
    root: Optional[TreeNode], p: TreeNode, q: TreeNode
) -> Optional[TreeNode]:
    if not root or root is p or root is q:
        return root
    left = lowest_common_ancestor(root.left, p, q)
    right = lowest_common_ancestor(root.right, p, q)
    if left and right:
        return root
    return left if left else right


def lowest_common_ancestor_bst(
    root: Optional[TreeNode], p: TreeNode, q: TreeNode
) -> Optional[TreeNode]:
    cur = root
    while cur:
        if p.val < cur.val and q.val < cur.val:
            cur = cur.left
        elif p.val > cur.val and q.val > cur.val:
            cur = cur.right
        else:
            return cur
    return None


# =============================================================================
# 11. Top K Frequent — 最小堆 O(n log k)
# =============================================================================

def top_k_frequent(nums: List[int], k: int) -> List[int]:
    counts = Counter(nums)
    heap: List[tuple[int, int]] = []
    for num, freq in counts.items():
        heapq.heappush(heap, (freq, num))
        if len(heap) > k:
            heapq.heappop(heap)
    return [num for _, num in heap]


# =============================================================================
# 12. K Closest Points — 大小为 k 的最大堆 O(n log k)
# =============================================================================

def k_closest(points: List[List[int]], k: int) -> List[List[int]]:
    heap: List[tuple[int, int, int]] = []
    for x, y in points:
        dist = x * x + y * y
        heapq.heappush(heap, (-dist, x, y))
        if len(heap) > k:
            heapq.heappop(heap)
    return [[x, y] for _, x, y in heap]


# =============================================================================
# 13. Longest Substring Without Repeating — 滑动窗口 O(n)
# =============================================================================

def length_of_longest_substring(s: str) -> int:
    last: dict[str, int] = {}
    left = 0
    best = 0
    for right, ch in enumerate(s):
        if ch in last and last[ch] >= left:
            left = last[ch] + 1
        last[ch] = right
        best = max(best, right - left + 1)
    return best


# =============================================================================
# 14. Permutations — 回溯 O(n * n!)
# =============================================================================

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


# =============================================================================
# 15. Binary Tree Level Order — BFS 按层 O(n)
# =============================================================================

def level_order(root: Optional[TreeNode]) -> List[List[int]]:
    if not root:
        return []
    result: List[List[int]] = []
    q: deque[TreeNode] = deque([root])
    while q:
        level_size = len(q)
        level: List[int] = []
        for _ in range(level_size):
            node = q.popleft()
            level.append(node.val)
            if node.left:
                q.append(node.left)
            if node.right:
                q.append(node.right)
        result.append(level)
    return result


# =============================================================================
# 16. SPSC Ring Buffer — 单生产者单消费者，无锁 O(1) push/pop
# =============================================================================

class SPSCRingBuffer(Generic[T]):
    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self.size = capacity + 1
        self.buf: List[Optional[T]] = [None] * self.size
        self.head = 0
        self.tail = 0

    @property
    def capacity(self) -> int:
        return self.size - 1

    def __len__(self) -> int:
        if self.tail >= self.head:
            return self.tail - self.head
        return self.size - self.head + self.tail

    def empty(self) -> bool:
        return self.head == self.tail

    def full(self) -> bool:
        return (self.tail + 1) % self.size == self.head

    def push(self, item: T) -> bool:
        next_tail = (self.tail + 1) % self.size
        if next_tail == self.head:
            return False
        self.buf[self.tail] = item
        self.tail = next_tail
        return True

    def pop(self) -> Optional[T]:
        if self.head == self.tail:
            return None
        item = self.buf[self.head]
        self.buf[self.head] = None
        self.head = (self.head + 1) % self.size
        return item
