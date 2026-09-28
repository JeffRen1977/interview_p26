#include "strategy.h"
#include "game_engine.h"

#include <stdexcept>

std::vector<std::array<int, 3>> all_triplet_ranks() {
    std::vector<std::array<int, 3>> out;
    for (int r1 = kMinRank; r1 <= kMaxRank; ++r1) {
        for (int r2 = r1; r2 <= kMaxRank; ++r2) {
            const int r3 = kTargetSum - r1 - r2;
            if (r2 <= r3 && r3 <= kMaxRank) out.push_back({r1, r2, r3});
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Phase 2
// ---------------------------------------------------------------------------
std::optional<Triplet> GreedyStrategy::choose_move(const Table& table) {
    (void)table;
    // TODO(Phase 2): return the first combination of 3 distinct POSITIONS on the
    // table whose ranks sum to kTargetSum, else nullopt.
    throw std::logic_error("Phase 2: implement GreedyStrategy::choose_move");
}

// ---------------------------------------------------------------------------
// Phase 3
// ---------------------------------------------------------------------------
long long count_triplets_sum_15(const std::vector<Card>& cards) {
    (void)cards;
    // TODO(Phase 3): frequency table over ranks + combinatorics. Keep every
    // intermediate in long long.
    throw std::logic_error("Phase 3: implement count_triplets_sum_15");
}

std::optional<Triplet> FastStrategy::choose_move(const Table& table) {
    (void)table;
    // TODO(Phase 3): bucket cards by rank, scan the 13 rank triples from
    // all_triplet_ranks(), and materialise one triplet from the buckets.
    throw std::logic_error("Phase 3: implement FastStrategy::choose_move");
}
