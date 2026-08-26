#include "recommenders.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

bool is_valid_recommendations(const SocialGraph& graph,
                              const std::string& user,
                              const std::vector<std::string>& candidates) {
    if (!graph.has_user(user)) return false;
    // Hoisted: this is the SOURCE user's friend set, which is the one rule 3
    // actually cares about.
    const FriendSet& own = graph.friends(user);
    std::unordered_set<std::string> seen;
    for (const std::string& candidate : candidates) {
        if (candidate == user) return false;
        // fix #1: ask whether the candidate is a friend of `user`, not of itself.
        // The graph is irreflexive, so the original branch could never fire.
        if (own.count(candidate)) return false;
        if (seen.count(candidate)) return false;
        // fix #2: rule 5 was never implemented at all.
        if (!graph.has_user(candidate)) return false;
        seen.insert(candidate);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Phase 2 -- scan every user, O(N * deg) per call
// ---------------------------------------------------------------------------
namespace {

std::size_t mutual_count(const FriendSet& own, const FriendSet& other) {
    // Iterate the smaller set: intersection cost is O(min(|a|, |b|)).
    const FriendSet& small = own.size() <= other.size() ? own : other;
    const FriendSet& large = own.size() <= other.size() ? other : own;
    std::size_t total = 0;
    for (const std::string& name : small) {
        if (large.count(name)) ++total;
    }
    return total;
}

std::vector<std::string> top_k(std::vector<std::pair<std::size_t, std::string>>& scored, int k) {
    // Score descending, then id ascending.
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                  return a.first != b.first ? a.first > b.first : a.second < b.second;
              });
    std::vector<std::string> out;
    for (const auto& [score, name] : scored) {
        if (static_cast<int>(out.size()) >= k) break;
        (void)score;
        out.push_back(name);
    }
    return out;
}

}  // namespace

std::vector<std::string> recommend_baseline(const SocialGraph& graph,
                                            const std::string& user,
                                            int k) {
    if (k <= 0 || !graph.has_user(user)) return {};
    const FriendSet& own = graph.friends(user);

    std::vector<std::pair<std::size_t, std::string>> scored;
    for (const std::string& other : graph.users()) {
        if (other == user || own.count(other)) continue;
        const std::size_t mutual = mutual_count(own, graph.friends(other));
        if (mutual > 0) scored.emplace_back(mutual, other);
    }
    return top_k(scored, k);
}

// ---------------------------------------------------------------------------
// Phase 3 -- offline evaluation
// ---------------------------------------------------------------------------
Metrics evaluate(const SocialGraph& graph,
                 const Recommender& recommend_fn,
                 const std::map<std::string, std::vector<std::string>>& holdout,
                 int k) {
    if (k <= 0) throw std::invalid_argument("k must be >= 1");
    if (holdout.empty()) return Metrics{};

    Metrics totals;
    // std::map iterates in sorted key order, so this is deterministic.
    for (const auto& [user, actual_list] : holdout) {
        const std::unordered_set<std::string> actual(actual_list.begin(), actual_list.end());
        const std::vector<std::string> recs = recommend_fn(graph, user, k);
        std::size_t hits = 0;
        for (const std::string& rec : recs) {
            if (actual.count(rec)) ++hits;
        }
        totals.precision += static_cast<double>(hits) / k;
        if (!actual.empty()) {
            totals.recall += static_cast<double>(hits) / static_cast<double>(actual.size());
        }
        if (!recs.empty()) totals.coverage += 1.0;
    }

    const double n = static_cast<double>(holdout.size());
    return Metrics{totals.precision / n, totals.recall / n, totals.coverage / n};
}

// ---------------------------------------------------------------------------
// Phase 4 -- 2-hop expansion
// ---------------------------------------------------------------------------
// A candidate can only score if it shares a friend with `user`, so the only
// nodes worth visiting are friends-of-friends. Counting how many friends of
// `user` point at each friend-of-friend IS the mutual-friend count:
//   f in friends(user) and c in friends(f)  <=>  f in friends(user) & friends(c)
//
// Cost is O(sum of deg(f) for f in friends(user)) instead of O(N * deg).
std::vector<std::string> recommend_fast(const SocialGraph& graph,
                                        const std::string& user,
                                        int k) {
    if (k <= 0 || !graph.has_user(user)) return {};
    const FriendSet& own = graph.friends(user);

    std::unordered_map<std::string, std::size_t> counts;
    for (const std::string& friend_id : own) {
        for (const std::string& candidate : graph.friends(friend_id)) {
            if (candidate == user || own.count(candidate)) continue;
            ++counts[candidate];
        }
    }

    std::vector<std::pair<std::size_t, std::string>> scored;
    scored.reserve(counts.size());
    for (const auto& [name, score] : counts) scored.emplace_back(score, name);
    return top_k(scored, k);
}
