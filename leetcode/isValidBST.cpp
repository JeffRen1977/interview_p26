bool dfs(TreeNode *root, int minVal, int maxVal)
{
  if(!root) return true;
  if(root->val<=minVal || root->val>=maxVal) return false;
  return dfs(root->left, minVal, root->val)&&dfs(root->right,root->val, maxVal);
}


bool isValidBST(TreeNode *root)
{
  return dfs(root,LLONG_MIN,LLONG_MAX);
}
