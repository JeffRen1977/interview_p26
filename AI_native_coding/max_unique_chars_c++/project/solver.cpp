#include "solver.h"

#include <bitset>
#include <stdexcept>

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
// Phase 2
// ---------------------------------------------------------------------------
int max_unique_length(const std::vector<std::string>& words) {
    (void)words;
    // TODO(Phase 2): sanitize, turn each candidate into a mask, then backtrack
    // over "take this word / skip this word".
    throw std::logic_error("Phase 2: implement max_unique_length");
}

// ---------------------------------------------------------------------------
// Phase 3
// ---------------------------------------------------------------------------
int max_unique_length_fast(const std::vector<std::string>& words) {
    (void)words;
    // TODO(Phase 3): work on 26-bit masks and deduplicate reachable STATES --
    // the number of distinct letter sets you can build is far smaller than the
    // number of word subsets.
    throw std::logic_error("Phase 3: implement max_unique_length_fast");
}
