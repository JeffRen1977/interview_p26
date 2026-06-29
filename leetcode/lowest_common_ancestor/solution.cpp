// LeetCode 236 - Lowest Common Ancestor.

#include <cassert>
#include <iostream>
#include "../common/tree_node.hpp"

TreeNode* lowestCommonAncestor(TreeNode* root, TreeNode* p, TreeNode* q) {
    if (!root || root == p || root == q) return root;
    TreeNode* left = lowestCommonAncestor(root->left, p, q);
    TreeNode* right = lowestCommonAncestor(root->right, p, q);
    if (left && right) return root;
    return left ? left : right;
}

TreeNode* lowestCommonAncestorBST(TreeNode* root, TreeNode* p, TreeNode* q) {
    TreeNode* cur = root;
    while (cur) {
        if (p->val < cur->val && q->val < cur->val) {
            cur = cur->left;
        } else if (p->val > cur->val && q->val > cur->val) {
            cur = cur->right;
        } else {
            return cur;
        }
    }
    return nullptr;
}

int main() {
    TreeNode* n7 = new TreeNode(7);
    TreeNode* n4 = new TreeNode(4);
    TreeNode* n2 = new TreeNode(2, n7, n4);
    TreeNode* n5 = new TreeNode(5, new TreeNode(6), n2);
    TreeNode* n1 = new TreeNode(1, new TreeNode(0), new TreeNode(8));
    TreeNode* root = new TreeNode(3, n5, n1);

    assert(lowestCommonAncestor(root, n5, n1)->val == 3);
    assert(lowestCommonAncestor(root, n5, n4)->val == 5);
    freeTree(root);
    std::cout << "lowest_common_ancestor: ok\n";
    return 0;
}
