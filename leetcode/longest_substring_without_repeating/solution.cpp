// LeetCode 3 - Longest Substring Without Repeating Characters.

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>

int lengthOfLongestSubstring(const std::string& s) {
    std::unordered_map<char, int> last;
    int left = 0, best = 0;
    for (int right = 0; right < static_cast<int>(s.size()); ++right) {
        char ch = s[right];
        if (last.count(ch) && last[ch] >= left) {
            left = last[ch] + 1;
        }
        last[ch] = right;
        best = std::max(best, right - left + 1);
    }
    return best;
}

int main() {
    assert(lengthOfLongestSubstring("abcabcbb") == 3);
    assert(lengthOfLongestSubstring("bbbbb") == 1);
    std::cout << "longest_substring_without_repeating: ok\n";
    return 0;
}
