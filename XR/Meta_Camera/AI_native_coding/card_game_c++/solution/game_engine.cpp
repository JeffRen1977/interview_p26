#include "game_engine.h"
#include "strategy.h"

bool GameEngine::is_valid_move(const std::vector<Card>& cards) const {
    if (cards.size() != static_cast<std::size_t>(kSetSize)) return false;

    int total = 0;
    for (const Card& card : cards) total += card.rank;
    if (total != kTargetSum) return false;

    // Phase 1 fix: availability is a MULTISET question. Comparing ranks lets a
    // card that isn't on the table through (7H vs 7S) and lets the same card be
    // consumed three times. Table::contains_all already does it right.
    return table_.contains_all(cards);
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
    while (max_moves < 0 || score_ < max_moves) {
        const std::optional<Triplet> move = strategy.choose_move(table_);
        if (!move.has_value()) break;
        const std::vector<Card> cards(move->begin(), move->end());
        apply_move(cards);  // re-validates: a buggy strategy must not score
    }
    return score_;
}
