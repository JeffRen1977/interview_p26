"""LeetCode 102 - Binary Tree Level Order Traversal. BFS."""

from __future__ import annotations

from collections import deque
from typing import List, Optional


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


if __name__ == "__main__":
    n7 = TreeNode(7)
    n4 = TreeNode(4)
    n2 = TreeNode(2, n7, n4)
    n5 = TreeNode(5, TreeNode(6), n2)
    n1 = TreeNode(1, TreeNode(0), TreeNode(8))
    root = TreeNode(3, n5, n1)
    assert level_order(root) == [[3], [5, 1], [6, 2, 0, 8], [7, 4]]
    print("binary_tree_level_order: ok")
