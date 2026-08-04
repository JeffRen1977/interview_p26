class TriNode {
   TriNode children[26];
   bool isEnd;
  TrieNode(){for(int i =0;i<26;i++)children[i]=nullptr;isEnd = false;}
]}
class Trie{
 TrieNode *root;
 Trie(){ root = new TriNode();}
 void insertWord(string word){
   TriNode *curr = root;
   for(int i = 0;i<word.size();i++)
  {
    if(curr.chilren[word[i]-'a']==nullptr)
      curr.chilren[word[i]-'a']=new TriNode();
   curr = curr.chilren[word[i]-'a'];
  }
  curr->isEnd = true;
 }
bool search(string word)
{
   TriNode *curr = root;
   for(int i = 0;i<word.size();i++)
  {
    if(curr.chilren[word[i]-'a']==NULL)
      return false;
   curr = curr.chilren[word[i]-'a'];
  }
 return curr->isEnd;
}
