#include "card.h"

#include <algorithm>
#include <stdexcept>

Card::Card(int rank_in, char suit_in) : rank(rank_in), suit(suit_in) {
    if (rank < kMinRank || rank > kMaxRank) {
        throw std::invalid_argument("rank must be in [1, 9], got " + std::to_string(rank));
    }
    if (std::find(kSuits.begin(), kSuits.end(), suit) == kSuits.end()) {
        throw std::invalid_argument(std::string("unknown suit: ") + suit);
    }
}
