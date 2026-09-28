// Move-selection strategies.
//
// Phase 2: GreedyStrategy         -- find any legal sum-15 triplet, brute force is fine
// Phase 3: count_triplets_sum_15  -- must stay fast on a 20,000-card table
//          FastStrategy           -- same behaviour as Greedy, O(9^3) per decision
#pragma once

#include "card.h"
#include "table.h"

#include <array>
#include <optional>
#include <vector>

using Triplet = std::array<Card, 3>;

// Contract used by GameEngine::play_out. Do not change this signature.
class Strategy {
public:
    virtual ~Strategy() = default;
    // Return one legal triplet from the table, or nullopt if there is none.
    virtual std::optional<Triplet> choose_move(const Table& table) = 0;
};

// Phase 2: first legal triplet in table order.
class GreedyStrategy : public Strategy {
public:
    std::optional<Triplet> choose_move(const Table& table) override;
};

// Phase 3: same behaviour as GreedyStrategy, but O(9^3) per decision.
class FastStrategy : public Strategy {
public:
    std::optional<Triplet> choose_move(const Table& table) override;
};

// How many distinct 3-card subsets (by POSITION) sum to 15.
//
// Must stay fast for 20,000+ cards, so O(n^3) enumeration is out.
// Hint: ranks only span 1..9.
//
// The return type is long long on purpose: C(6000, 3) is about 3.6e10, and the
// intermediate f*(f-1)*(f-2) is larger still. 32-bit arithmetic overflows here,
// and signed overflow is undefined behaviour, not a wrap you can rely on.
long long count_triplets_sum_15(const std::vector<Card>& cards);

// Every non-decreasing rank triple (r1<=r2<=r3) in 1..9 summing to 15.
// Given and correct -- there are exactly 13 of them.
std::vector<std::array<int, 3>> all_triplet_ranks();
