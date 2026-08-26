#include "table.h"

#include <algorithm>
#include <stdexcept>

std::map<Card, int> Table::counts() const {
    std::map<Card, int> out;
    for (const Card& card : cards_) ++out[card];
    return out;
}

bool Table::contains_all(const std::vector<Card>& cards) const {
    std::map<Card, int> need;
    for (const Card& card : cards) ++need[card];
    const std::map<Card, int> have = counts();
    for (const auto& [card, n] : need) {
        const auto it = have.find(card);
        if (it == have.end() || it->second < n) return false;
    }
    return true;
}

void Table::remove(const std::vector<Card>& cards) {
    if (!contains_all(cards)) {
        throw std::invalid_argument("cannot remove cards that are not on the table");
    }
    for (const Card& card : cards) {
        const auto it = std::find(cards_.begin(), cards_.end(), card);
        cards_.erase(it);
    }
}

void Table::add(const std::vector<Card>& cards) {
    cards_.insert(cards_.end(), cards.begin(), cards.end());
}
