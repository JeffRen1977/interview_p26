// People-You-May-Know: validation, ranking, evaluation. Reference implementation.
#pragma once

#include "social_graph.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

using Recommender = std::function<std::vector<std::string>(const SocialGraph&, const std::string&, int)>;

// True iff `candidates` is a legal recommendation list for `user`.
//
// All five rules must hold:
//   1. `user` must exist in the graph
//   2. `user` must not be recommended to themselves
//   3. Nobody who is already a friend of `user` may be recommended
//   4. No candidate may appear twice
//   5. Every candidate must exist in the graph
//
// An empty candidate list is always valid (for a user who exists).
bool is_valid_recommendations(const SocialGraph& graph,
                              const std::string& user,
                              const std::vector<std::string>& candidates);

// Top-`k` people-you-may-know for `user`, ranked by mutual friend count.
//
// Score of a candidate c is |friends(user) & friends(c)|.
// Only candidates with a score >= 1 are eligible, and the result must always
// satisfy is_valid_recommendations (no self, no existing friends, no dupes).
//
// Order: score DESCENDING, then user id ASCENDING as the tie-break.
// Unknown `user` or k <= 0 returns {}.
std::vector<std::string> recommend_baseline(const SocialGraph& graph,
                                            const std::string& user,
                                            int k = 5);

struct Metrics {
    double precision = 0.0;
    double recall = 0.0;
    double coverage = 0.0;
};

// Score a recommender against held-out edges.
//
// `holdout` maps a user to the friendships that were hidden from `graph`.
// For every user in `holdout`, call recommend_fn(graph, user, k) and let `hits`
// be how many of the returned ids are in that user's holdout set.
//
// Returns the mean over holdout users of:
//   precision -- hits / k                      (always divide by k, not by recs.size())
//   recall    -- hits / holdout[user].size()   (0.0 when the holdout set is empty)
//   coverage  -- 1.0 if the recommender returned anything at all, else 0.0
//
// An empty `holdout` scores 0.0 on all three. k <= 0 throws std::invalid_argument.
Metrics evaluate(const SocialGraph& graph,
                 const Recommender& recommend_fn,
                 const std::map<std::string, std::vector<std::string>>& holdout,
                 int k = 3);

// Identical output to recommend_baseline, but it has to survive the stress set:
// 40k users queried thousands of times. Do not touch users that cannot score.
std::vector<std::string> recommend_fast(const SocialGraph& graph,
                                        const std::string& user,
                                        int k = 5);
