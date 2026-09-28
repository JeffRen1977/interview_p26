// The pool of cards face-up on the table. Given -- do not change this file.
#pragma once

#include "card.h"

#include <map>
#include <vector>

// A MULTISET of cards.
//
// The same (rank, suit) can appear more than once, so every containment or
// removal check must be done on *counts*, not on plain membership.
class Table {
public:
    Table() = default;
    explicit Table(std::vector<Card> cards) : cards_(std::move(cards)) {}

    // A copy -- callers must not mutate the table through this vector.
    std::vector<Card> cards() const { return cards_; }

    // Multiset view: Card -> how many copies are on the table.
    std::map<Card, int> counts() const;

    // True only if the table holds enough copies of every requested card.
    // contains_all({c, c}) is false when only one copy of c is on the table.
    bool contains_all(const std::vector<Card>& cards) const;

    // Throws std::invalid_argument if contains_all is false.
    void remove(const std::vector<Card>& cards);

    void add(const std::vector<Card>& cards);

    std::size_t size() const { return cards_.size(); }
    bool empty() const { return cards_.empty(); }

private:
    std::vector<Card> cards_;
};
