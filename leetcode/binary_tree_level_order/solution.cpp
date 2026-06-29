// LeetCode 102 - Binary Tree Level Order Traversal.

#include <cassert>
#include <iostream>
#include <queue>
#include <vector>
#include "../common/tree_node.hpp"

std::vector<std::vector<int>> levelOrder(TreeNode* root) {
    std::vector<std::vector<int>> result;
    if (!root) return result;

    std::queue<TreeNode*> q;
    q.push(root);
    while (!q.empty()) {
        int levelSize = static_cast<int>(q.size());
        std::vector<int> level;
        for (int i = 0; i < levelSize; ++i) {
            TreeNode* node = q.front();
            q.pop();
            level.push_back(node->val);
            if (node->left) q.push(node->left);
            if (node->right) q.push(node->right);
        }
        result.push_back(std::move(level));
    }
    return result;
}

int main() {
    TreeNode* n7 = new TreeNode(7);
    TreeNode* n4 = new TreeNode(4);
    TreeNode* n2 = new TreeNode(2, n7, n4);
    TreeNode* n5 = new TreeNode(5, new TreeNode(6), n2);
    TreeNode* n1 = new TreeNode(1, new TreeNode(0), new TreeNode(8));
    TreeNode* root = new TreeNode(3, n5, n1);

    auto got = levelOrder(root);
    assert(got.size() == 4);
    assert(got[0] == std::vector<int>({3}));
    freeTree(root);
    std::cout << "binary_tree_level_order: ok\n";
    return 0;
}
