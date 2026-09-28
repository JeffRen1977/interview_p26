#include "wordlist.h"

#include <cctype>

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first);
}

bool is_ascii_lower(char ch) { return ch >= 'a' && ch <= 'z'; }

}  // namespace

std::vector<std::string> sanitize(const std::vector<std::string>& words) {
    std::vector<std::string> cleaned;
    for (const std::string& raw : words) {
        std::string word = trim(raw);
        if (word.empty()) continue;

        bool ok = true;
        int seen = 0;
        for (char ch : word) {
            if (!is_ascii_lower(ch)) {
                ok = false;
                break;
            }
            const int bit = 1 << (ch - 'a');
            if (seen & bit) {
                ok = false;
                break;
            }
            seen |= bit;
        }
        if (ok) cleaned.push_back(word);
    }
    return cleaned;
}

int unique_char_count(const std::string& text) {
    return static_cast<int>(text.size());
}
