#include "wordlist.h"

#include <cctype>
#include <unordered_set>

namespace {

// Phase 1 fix #1: trim BOTH ends. Stripping only the front leaves "\tXYZ\n" as
// "XYZ\n", which then fails the a-z check and is dropped entirely.
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool is_ascii_lower(char ch) { return ch >= 'a' && ch <= 'z'; }

}  // namespace

std::vector<std::string> sanitize(const std::vector<std::string>& words) {
    std::vector<std::string> cleaned;
    for (const std::string& raw : words) {
        std::string word = trim(raw);
        // Phase 1 fix #2: lower-case BEFORE the duplicate check, otherwise "aA"
        // looks like two distinct characters and survives.
        for (char& ch : word) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
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
    // Phase 1 fix #3: distinct characters, not length.
    const std::unordered_set<char> distinct(text.begin(), text.end());
    return static_cast<int>(distinct.size());
}
