// Phase 3: Monte-Carlo evaluation of a strategy over many random tables.
#pragma once

#include "card.h"
#include "rng.h"
#include "strategy.h"

#include <vector>

struct MonteCarloStats {
    int games = 0;
    long long total_score = 0;
    double mean_score = 0.0;
    int max_score = 0;
    int zero_score_games = 0;

    bool operator==(const MonteCarloStats& o) const {
        return games == o.games && total_score == o.total_score &&
               mean_score == o.mean_score && max_score == o.max_score &&
               zero_score_games == o.zero_score_games;
    }
};

// Deterministic random table given an rng seeded by the caller. Given.
std::vector<Card> deal(Rng& rng, int size);

// Run `games` independent tables and report score statistics.
// Must be deterministic for a given seed: same seed -> identical struct.
MonteCarloStats monte_carlo(Strategy& strategy, int games, int table_size, unsigned seed);
