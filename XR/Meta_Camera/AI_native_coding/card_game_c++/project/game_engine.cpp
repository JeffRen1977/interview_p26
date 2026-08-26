#include "game_engine.h"
#include "strategy.h"

#include <algorithm>

bool GameEngine::is_valid_move(const std::vector<Card>& cards) const {
    if (cards.size() != static_cast<std::size_t>(kSetSize)) return false;

    int total = 0;
    for (const Card& card : cards) total += card.rank;
    if (total != kTargetSum) return false;

    std::vector<int> table_ranks;
    for (const Card& card : table_.cards()) table_ranks.push_back(card.rank);
    for (const Card& card : cards) {
        if (std::find(table_ranks.begin(), table_ranks.end(), card.rank) == table_ranks.end()) {
            return false;
        }
    }
    return true;
}

int GameEngine::apply_move(const std::vector<Card>& cards) {
    if (!is_valid_move(cards)) {
        std::string detail;
        for (const Card& card : cards) detail += card.str() + " ";
        throw InvalidMove("illegal move: " + detail);
    }
    table_.remove(cards);
    ++score_;
    return score_;
}

// ---------------------------------------------------------------------------
// Phase 2
// ---------------------------------------------------------------------------
int GameEngine::play_out(Strategy& strategy, int max_moves) {
    (void)strategy;
    (void)max_moves;
    // TODO(Phase 2): loop strategy.choose_move(table_), validate and apply each
    // move, stop when it returns nullopt (or max_moves is reached).
    throw std::logic_error("Phase 2: implement play_out");
}
