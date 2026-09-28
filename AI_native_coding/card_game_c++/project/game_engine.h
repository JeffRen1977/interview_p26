// Rule validation and round settlement.
//
// Phase 1 lives in game_engine.cpp: is_valid_move has a logic hole. Find it
// from the failing assertions before you touch anything.
#pragma once

#include "card.h"
#include "table.h"

#include <optional>
#include <stdexcept>
#include <vector>

inline constexpr int kTargetSum = 15;
inline constexpr int kSetSize = 3;

class Strategy;  // strategy.h

// Raised when apply_move is given a move the rules reject.
struct InvalidMove : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

class GameEngine {
public:
    explicit GameEngine(Table& table) : table_(table) {}

    // A move is legal iff it is exactly kSetSize cards, every one of them is
    // actually AVAILABLE on the table (counts, not ranks), and their ranks sum
    // to kTargetSum.
    bool is_valid_move(const std::vector<Card>& cards) const;

    // Validate, remove the cards from the table, and award one point.
    // Throws InvalidMove if the move is illegal.
    int apply_move(const std::vector<Card>& cards);

    // Ask the strategy for moves until it has none left; return total score.
    // `max_moves < 0` means unlimited.
    int play_out(Strategy& strategy, int max_moves = -1);

    int score() const { return score_; }

private:
    Table& table_;
    int score_ = 0;
};
