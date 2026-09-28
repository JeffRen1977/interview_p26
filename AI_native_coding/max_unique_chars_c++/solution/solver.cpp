#include "solver.h"
#include "wordlist.h"

#include <bitset>
#include <unordered_set>

int word_mask(const std::string& word) {
    int mask = 0;
    for (char ch : word) {
        if (ch >= 'a' && ch <= 'z') {
            mask |= 1 << (ch - 'a');
        }
    }
    return mask;
}

int popcount(int mask) {
    return static_cast<int>(std::bitset<32>(static_cast<unsigned>(mask)).count());
}

// ---------------------------------------------------------------------------
// Phase 2 -- backtracking
// ---------------------------------------------------------------------------
namespace {

void backtrack(const std::vector<int>& masks, std::size_t idx, int state, int length, int& best) {
    if (length > best) best = length;
    for (std::size_t i = idx; i < masks.size(); ++i) {
        if ((state & masks[i]) == 0) {
            backtrack(masks, i + 1, state | masks[i], length + popcount(masks[i]), best);
        }
    }
}

}  // namespace

int max_unique_length(const std::vector<std::string>& words) {
    std::vector<int> masks;
    for (const std::string& word : sanitize(words)) {
        masks.push_back(word_mask(word));
    }
    if (masks.empty()) return 0;

    int best = 0;
    backtrack(masks, 0, 0, 0, best);
    return best;
}

// ---------------------------------------------------------------------------
// Phase 3 -- deduplicate STATES, not subsets
// ---------------------------------------------------------------------------
// The subset space is 2^n, but two different subsets that cover the same set of
// letters are the same state, and there are at most 2^26 letter sets. Folding
// duplicate words into one mask collapses the input as well.
int max_unique_length_fast(const std::vector<std::string>& words) {
    std::unordered_set<int> unique_masks;
    for (const std::string& word : sanitize(words)) {
        unique_masks.insert(word_mask(word));
    }
    if (unique_masks.empty()) return 0;

    std::unordered_set<int> reachable{0};
    std::vector<int> fresh;
    int best = 0;

    for (int mask : unique_masks) {
        fresh.clear();
        for (int state : reachable) {
            if ((state & mask) == 0) {
                const int combined = state | mask;
                fresh.push_back(combined);
                const int count = popcount(combined);
                if (count > best) {
                    best = count;
                    if (best == 26) return 26;  // upper bound, stop early
                }
            }
        }
        reachable.insert(fresh.begin(), fresh.end());
    }
    return best;
}
