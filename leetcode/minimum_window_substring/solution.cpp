// LeetCode 76 - Minimum Window Substring.
// Sliding window: expand right, shrink left while t is covered.

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>

bool isValid(const std::unordered_map<char, int>& pattern,
             const std::unordered_map<char, int>& target) {
    for (auto it = pattern.begin(); it != pattern.end(); ++it) {
        auto found = target.find(it->first);
        if (found == target.end()) {
            return false;
        }
        if (it->second > found->second) {
            return false;
        }
    }
    return true;
}

std::string minWindow(const std::string& s, const std::string& t) {
    std::unordered_map<char, int> pattern;
    for (char ch : t) {
        pattern[ch]++;
    }

    int left = 0;
    std::unordered_map<char, int> target;
    std::string res;

    for (int right = 0; right < static_cast<int>(s.size()); ++right) {
        target[s[right]]++;
        while (isValid(pattern, target)) {
            int len = right - left + 1;
            if (res.empty() || len < static_cast<int>(res.size())) {
                res = s.substr(left, len);
            }
            target[s[left]]--;
            ++left;
        }
    }
    return res;
}

int main() {
    assert(minWindow("ADOBECODEBANC", "ABC") == "BANC");
    assert(minWindow("a", "a") == "a");
    assert(minWindow("a", "aa") == "");
    assert(minWindow("ab", "b") == "b");
    std::cout << "minimum_window_substring: ok\n";
    return 0;
}
