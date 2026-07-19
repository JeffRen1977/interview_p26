class Codec {
public:

    // Encodes a tree to a single string.
    string serialize(TreeNode* root) {
        if(!root) return " #";
        return to_string(root->val) +" "+serialize(root->left)+" "+serialize(root->right);
    }

    // Decodes your encoded data to tree.
     TreeNode* deserialize(string data) {
     istringstream is(data);
     return DFS(is);
    
    }
private:
    TreeNode*DFS(istringstream& is)
    {
         string token;
         is>>token;
    
        if(token=="#") return NULL;
        TreeNode*root = new TreeNode(stoll(token));
        root->left = DFS(is);
        root->right = DFS(is);
        return root;
    }
};
