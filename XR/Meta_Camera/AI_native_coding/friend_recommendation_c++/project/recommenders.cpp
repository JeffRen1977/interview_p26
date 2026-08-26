#include "recommenders.h"

#include <stdexcept>
#include <unordered_set>

bool is_valid_recommendations(const SocialGraph& graph,
                              const std::string& user,
                              const std::vector<std::string>& candidates) {
    if (!graph.has_user(user)) return false;
    std::unordered_set<std::string> seen;
    for (const std::string& candidate : candidates) {
        if (candidate == user) return false;
        if (graph.friends(candidate).count(candidate)) return false;
        if (seen.count(candidate)) return false;
        seen.insert(candidate);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Phase 2
// ---------------------------------------------------------------------------
std::vector<std::string> recommend_baseline(const SocialGraph& graph,
                                            const std::string& user,
                                            int k) {
    (void)graph;
    (void)user;
    (void)k;
    // TODO(Phase 2): scanning graph.users() is the expected answer here.
    throw std::logic_error("Phase 2: implement recommend_baseline");
}

// ---------------------------------------------------------------------------
// Phase 3
// ---------------------------------------------------------------------------
Metrics evaluate(const SocialGraph& graph,
                 const Recommender& recommend_fn,
                 const std::map<std::string, std::vector<std::string>>& holdout,
                 int k) {
    (void)graph;
    (void)recommend_fn;
    (void)holdout;
    (void)k;
    // TODO(Phase 3): implement the three metrics.
    throw std::logic_error("Phase 3: implement evaluate");
}

// ---------------------------------------------------------------------------
// Phase 4
// ---------------------------------------------------------------------------
std::vector<std::string> recommend_fast(const SocialGraph& graph,
                                        const std::string& user,
                                        int k) {
    (void)graph;
    (void)user;
    (void)k;
    // TODO(Phase 4): the stress graph has 40k users and the loop queries it
    // thousands of times. Do not touch users who cannot possibly score.
    throw std::logic_error("Phase 4: implement recommend_fast");
}
