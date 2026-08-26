// Immutable playing card. Given -- do not change this file.
#pragma once

#include <array>
#include <string>

inline constexpr std::array<char, 4> kSuits = {'S', 'H', 'D', 'C'};
inline constexpr int kMinRank = 1;
inline constexpr int kMaxRank = 9;

// A card with a rank in [1, 9] and a suit.
//
// Value type: two cards with the same (rank, suit) are equal, and the table may
// legitimately hold several of them because the shoe is built from several
// decks. Ordered so it can be a std::map key.
struct Card {
    int rank;
    char suit;

    // Throws std::invalid_argument on an out-of-range rank or unknown suit.
    Card(int rank, char suit = 'S');

    bool operator==(const Card& other) const {
        return rank == other.rank && suit == other.suit;
    }
    bool operator!=(const Card& other) const { return !(*this == other); }
    bool operator<(const Card& other) const {
        return rank != other.rank ? rank < other.rank : suit < other.suit;
    }

    std::string str() const { return std::to_string(rank) + suit; }
};
