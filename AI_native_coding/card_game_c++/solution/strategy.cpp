#include "strategy.h"
#include "game_engine.h"

#include <map>
#include <vector>

std::vector<std::array<int, 3>> all_triplet_ranks() {
    std::vector<std::array<int, 3>> out;
    for (int r1 = kMinRank; r1 <= kMaxRank; ++r1) {
        for (int r2 = r1; r2 <= kMaxRank; ++r2) {
            const int r3 = kTargetSum - r1 - r2;
            if (r2 <= r3 && r3 <= kMaxRank) out.push_back({r1, r2, r3});
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Phase 2 -- first legal triplet by position
// ---------------------------------------------------------------------------
std::optional<Triplet> GreedyStrategy::choose_move(const Table& table) {
    const std::vector<Card> cards = table.cards();
    const std::size_t n = cards.size();
    for (std::size_t i = 0; i + 2 < n; ++i) {
        for (std::size_t j = i + 1; j + 1 < n; ++j) {
            for (std::size_t k = j + 1; k < n; ++k) {
                if (cards[i].rank + cards[j].rank + cards[k].rank == kTargetSum) {
                    return Triplet{cards[i], cards[j], cards[k]};
                }
            }
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Phase 3 -- frequency table + binomials
// ---------------------------------------------------------------------------
namespace {

// C(n, k) for k in {1, 2, 3}. Everything stays in long long: C(6000, 3) is
// ~3.6e10 and the numerator n*(n-1)*(n-2) is ~2.2e11, both far past int32.
long long choose3(long long n) { return n < 3 ? 0 : n * (n - 1) * (n - 2) / 6; }
long long choose2(long long n) { return n < 2 ? 0 : n * (n - 1) / 2; }

std::array<long long, kMaxRank + 1> rank_freq(const std::vector<Card>& cards) {
    std::array<long long, kMaxRank + 1> freq{};
    for (const Card& card : cards) ++freq[static_cast<std::size_t>(card.rank)];
    return freq;
}

}  // namespace

// Ranks only span 1..9, so the search space is not the cards, it is the ranks:
// exactly 13 non-decreasing triples sum to 15. Counting is O(n) to build the
// frequency table plus O(13) to walk it -- independent of how many valid
// combinations there are.
long long count_triplets_sum_15(const std::vector<Card>& cards) {
    const auto freq = rank_freq(cards);
    long long total = 0;
    for (const auto& [r1, r2, r3] : all_triplet_ranks()) {
        const long long f1 = freq[static_cast<std::size_t>(r1)];
        const long long f2 = freq[static_cast<std::size_t>(r2)];
        const long long f3 = freq[static_cast<std::size_t>(r3)];
        if (r1 == r2 && r2 == r3) {
            total += choose3(f1);            // (5,5,5)
        } else if (r1 == r2) {
            total += choose2(f1) * f3;       // (1,7,7) shape, low pair
        } else if (r2 == r3) {
            total += f1 * choose2(f2);       // (3,6,6) shape, high pair
        } else {
            total += f1 * f2 * f3;
        }
    }
    return total;
}

std::optional<Triplet> FastStrategy::choose_move(const Table& table) {
    std::map<int, std::vector<Card>> buckets;
    for (const Card& card : table.cards()) buckets[card.rank].push_back(card);

    for (const auto& [r1, r2, r3] : all_triplet_ranks()) {
        std::map<int, int> need;
        ++need[r1];
        ++need[r2];
        ++need[r3];
        bool ok = true;
        for (const auto& [rank, n] : need) {
            const auto it = buckets.find(rank);
            if (it == buckets.end() || static_cast<int>(it->second.size()) < n) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        std::vector<Card> picked;
        for (const auto& [rank, n] : need) {
            const std::vector<Card>& bucket = buckets.at(rank);
            picked.insert(picked.end(), bucket.begin(), bucket.begin() + n);
        }
        return Triplet{picked[0], picked[1], picked[2]};
    }
    return std::nullopt;
}
