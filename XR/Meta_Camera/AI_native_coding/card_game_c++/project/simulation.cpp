#include "simulation.h"
#include "game_engine.h"
#include "table.h"

#include <stdexcept>

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

MonteCarloStats monte_carlo(Strategy& strategy, int games, int table_size, unsigned seed) {
    (void)strategy;
    (void)games;
    (void)table_size;
    (void)seed;
    // TODO(Phase 3): build one Rng(seed), deal each table, play it out with the
    // strategy, and fill in games / total_score / mean_score / max_score /
    // zero_score_games.
    throw std::logic_error("Phase 3: implement monte_carlo");
}
