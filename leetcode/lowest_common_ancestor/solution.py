"""LeetCode 236 - Lowest Common Ancestor of a Binary Tree."""

from __future__ import annotations

from typing import Optional


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


if __name__ == "__main__":
    n7 = TreeNode(7)
    n4 = TreeNode(4)
    n2 = TreeNode(2, n7, n4)
    n5 = TreeNode(5, TreeNode(6), n2)
    n1 = TreeNode(1, TreeNode(0), TreeNode(8))
    root = TreeNode(3, n5, n1)
    assert lowest_common_ancestor(root, n5, n1).val == 3
    assert lowest_common_ancestor(root, n5, n4).val == 5
    print("lowest_common_ancestor: ok")
