#include "simulation.h"
#include "game_engine.h"
#include "table.h"

#include <algorithm>

std::vector<Card> deal(Rng& rng, int size) {
    std::vector<Card> cards;
    cards.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        const int rank = rng.next_int(kMinRank, kMaxRank);
        const char suit = kSuits[static_cast<std::size_t>(rng.next_int(0, 3))];
        cards.emplace_back(rank, suit);
    }
    return cards;
}

// One Rng threaded through the whole run, seeded exactly once: that is what
// makes the result reproducible. Never reach for a global generator or the
// clock here -- the determinism test is checking precisely that.
MonteCarloStats monte_carlo(Strategy& strategy, int games, int table_size, unsigned seed) {
    Rng rng(seed);
    MonteCarloStats stats;
    stats.games = games;

    for (int i = 0; i < games; ++i) {
        Table table(deal(rng, table_size));
        GameEngine engine(table);
        const int score = engine.play_out(strategy);
        stats.total_score += score;
        stats.max_score = std::max(stats.max_score, score);
        if (score == 0) ++stats.zero_score_games;
    }
    stats.mean_score = games > 0 ? static_cast<double>(stats.total_score) / games : 0.0;
    return stats;
}
