// Staged spec for the friend-recommendation task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "recommenders.h"
#include "social_graph.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_ok = 0;
int g_fail = 0;
int g_error = 0;

struct AssertFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void fail(const std::string& msg) { throw AssertFail(msg); }

void expect(bool cond, const std::string& msg) {
    if (!cond) fail(msg);
}

std::string show(const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ", ";
        out += "\"" + items[i] + "\"";
    }
    return out + "]";
}

void expect_eq_vec(const std::vector<std::string>& got,
                   const std::vector<std::string>& want,
                   const std::string& msg) {
    if (got != want) fail(msg + "\n  got=" + show(got) + "\n  want=" + show(want));
}

void expect_close(double got, double want, const std::string& msg) {
    if (std::fabs(got - want) > 1e-9) {
        std::ostringstream oss;
        oss << msg << " got=" << std::setprecision(12) << got << " want=" << want;
        fail(oss.str());
    }
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string user_id(int i) {
    std::ostringstream oss;
    oss << 'u' << std::setw(5) << std::setfill('0') << i;
    return oss.str();
}

// --- fixtures --------------------------------------------------------------

// alice-bob-carol cluster plus a far-away pair.
//
//     alice - bob      alice's friends = {bob, carol}
//     alice - carol    dave shares BOTH of them
//     bob   - dave
//     carol - dave
//     dave  - erin
//     frank - grace    (disconnected component)
SocialGraph tiny_graph() {
    SocialGraph graph;
    graph.add_friendship("alice", "bob");
    graph.add_friendship("alice", "carol");
    graph.add_friendship("bob", "dave");
    graph.add_friendship("carol", "dave");
    graph.add_friendship("dave", "erin");
    graph.add_friendship("frank", "grace");
    return graph;
}

SocialGraph random_graph(unsigned seed, int n_users, int edges_per_user) {
    std::mt19937 rng(seed);
    std::vector<std::string> ids;
    ids.reserve(static_cast<std::size_t>(n_users));
    for (int i = 0; i < n_users; ++i) ids.push_back(user_id(i));

    SocialGraph graph(ids);
    for (const std::string& id : ids) {
        for (int e = 0; e < edges_per_user; ++e) {
            const std::string& other = ids[rng() % static_cast<unsigned>(n_users)];
            if (other != id) graph.add_friendship(id, other);
        }
    }
    return graph;
}

// ---------------------------------------------------------------------------
// Phase 1 -- validation rules
// ---------------------------------------------------------------------------
void test_accepts_clean_candidate_list() {
    const SocialGraph g = tiny_graph();
    expect(is_valid_recommendations(g, "alice", {"dave", "erin"}), "dave+erin is legal");
}

void test_empty_candidate_list_is_valid() {
    const SocialGraph g = tiny_graph();
    expect(is_valid_recommendations(g, "alice", {}), "empty list is legal");
}

void test_rejects_self_recommendation() {
    const SocialGraph g = tiny_graph();
    expect(!is_valid_recommendations(g, "alice", {"dave", "alice"}), "alice recommended to alice");
}

void test_rejects_duplicates() {
    const SocialGraph g = tiny_graph();
    expect(!is_valid_recommendations(g, "alice", {"dave", "dave"}), "dave twice");
}

void test_rejects_unknown_source_user() {
    const SocialGraph g = tiny_graph();
    expect(!is_valid_recommendations(g, "nobody", {}), "unknown source user");
}

void test_rejects_existing_friend() {
    // bob is already alice's friend, so he is not a recommendation.
    const SocialGraph g = tiny_graph();
    expect(!is_valid_recommendations(g, "alice", {"bob"}), "bob is already a friend");
    expect(!is_valid_recommendations(g, "alice", {"dave", "carol"}), "carol is already a friend");
}

void test_rejects_unknown_candidate() {
    // Ids that are not in the graph cannot be recommended.
    const SocialGraph g = tiny_graph();
    expect(!is_valid_recommendations(g, "alice", {"ghost"}), "ghost is not in the graph");
    expect(!is_valid_recommendations(g, "alice", {"dave", "ghost"}), "ghost hidden in the list");
}

// ---------------------------------------------------------------------------
// Phase 2 -- baseline ranking
// ---------------------------------------------------------------------------
void test_ranks_by_mutual_friend_count() {
    // dave shares bob and carol with alice (2); erin shares nobody.
    const SocialGraph g = tiny_graph();
    expect_eq_vec(recommend_baseline(g, "alice", 5), {"dave"}, "alice's recommendations");
}

void test_excludes_self_and_existing_friends() {
    const SocialGraph g = tiny_graph();
    expect_eq_vec(recommend_baseline(g, "dave", 5), {"alice"}, "dave's recommendations");
}

void test_ties_break_by_user_id() {
    SocialGraph g;
    g.add_friendship("me", "hub");
    for (const std::string& name : {"zoe", "adam", "mia"}) g.add_friendship("hub", name);
    expect_eq_vec(recommend_baseline(g, "me", 3), {"adam", "mia", "zoe"}, "tie-break by id");
}

void test_respects_k() {
    SocialGraph g;
    g.add_friendship("me", "hub");
    for (const std::string& name : {"zoe", "adam", "mia"}) g.add_friendship("hub", name);
    expect_eq_vec(recommend_baseline(g, "me", 2), {"adam", "mia"}, "k = 2");
    expect_eq_vec(recommend_baseline(g, "me", 0), {}, "k = 0");
    expect_eq_vec(recommend_baseline(g, "me", -1), {}, "negative k");
}

void test_unknown_user_returns_empty() {
    const SocialGraph g = tiny_graph();
    expect_eq_vec(recommend_baseline(g, "nobody", 5), {}, "unknown user");
}

void test_no_mutual_friends_returns_empty() {
    SocialGraph g = tiny_graph();
    g.add_user("loner");
    expect_eq_vec(recommend_baseline(g, "loner", 5), {}, "isolated node");
    expect_eq_vec(recommend_baseline(g, "frank", 5), {}, "frank only knows grace");
}

void test_output_passes_the_phase1_validator() {
    const SocialGraph g = tiny_graph();
    for (const std::string& user : g.users()) {
        const auto recs = recommend_baseline(g, user, 5);
        expect(is_valid_recommendations(g, user, recs),
               user + " -> " + show(recs) + " is not a legal recommendation list");
    }
}

// ---------------------------------------------------------------------------
// Phase 3 -- offline evaluation
// ---------------------------------------------------------------------------
Recommender fixed(std::vector<std::string> recs) {
    return [recs](const SocialGraph&, const std::string&, int) { return recs; };
}

Recommender baseline_fn() {
    return [](const SocialGraph& g, const std::string& u, int k) {
        return recommend_baseline(g, u, k);
    };
}

void test_perfect_recommender_scores_one() {
    const SocialGraph g = tiny_graph();
    const Metrics m = evaluate(g, fixed({"dave"}), {{"alice", {"dave"}}}, 1);
    expect_close(m.precision, 1.0, "precision");
    expect_close(m.recall, 1.0, "recall");
    expect_close(m.coverage, 1.0, "coverage");
}

void test_precision_divides_by_k_not_by_len_recs() {
    // One hit out of k=4 slots is 0.25, even though only one id came back.
    const SocialGraph g = tiny_graph();
    const Metrics m = evaluate(g, fixed({"dave"}), {{"alice", {"dave"}}}, 4);
    expect_close(m.precision, 0.25, "precision must divide by k");
    expect_close(m.recall, 1.0, "recall");
}

void test_recall_uses_holdout_size() {
    const SocialGraph g = tiny_graph();
    const Metrics m = evaluate(g, fixed({"dave", "grace"}), {{"alice", {"dave", "erin"}}}, 2);
    expect_close(m.precision, 0.5, "precision");
    expect_close(m.recall, 0.5, "recall");
}

void test_metrics_average_over_users() {
    // alice -> ["dave"] hits; frank has no 2-hop neighbour so it returns {}.
    const SocialGraph g = tiny_graph();
    const Metrics m = evaluate(g, baseline_fn(), {{"alice", {"dave"}}, {"frank", {"erin"}}}, 1);
    expect_close(m.precision, 0.5, "precision");
    expect_close(m.recall, 0.5, "recall");
    expect_close(m.coverage, 0.5, "coverage");
}

void test_empty_holdout_is_zero() {
    const SocialGraph g = tiny_graph();
    const Metrics m = evaluate(g, baseline_fn(), {}, 3);
    expect_close(m.precision, 0.0, "precision");
    expect_close(m.recall, 0.0, "recall");
    expect_close(m.coverage, 0.0, "coverage");
}

void test_non_positive_k_is_rejected() {
    const SocialGraph g = tiny_graph();
    bool threw = false;
    try {
        evaluate(g, baseline_fn(), {{"alice", {"dave"}}}, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "k <= 0 must throw std::invalid_argument");
}

// ---------------------------------------------------------------------------
// Phase 4 -- scale
// ---------------------------------------------------------------------------
void test_same_answers_as_the_baseline() {
    const SocialGraph g = random_graph(1234, 400, 3);
    const auto ids = g.users();
    for (std::size_t i = 0; i < ids.size() && i < 120; ++i) {
        expect_eq_vec(recommend_fast(g, ids[i], 5), recommend_baseline(g, ids[i], 5),
                      "fast and baseline disagree for " + ids[i]);
    }
    expect_eq_vec(recommend_fast(g, "nobody", 5), {}, "unknown user");
    expect_eq_vec(recommend_fast(g, ids[0], 0), {}, "k = 0");
}

void test_fast_output_passes_the_validator() {
    const SocialGraph g = random_graph(99, 200, 3);
    for (const std::string& user : g.users()) {
        expect(is_valid_recommendations(g, user, recommend_fast(g, user, 5)),
               "illegal recommendation list for " + user);
    }
}

void test_top_k_is_a_prefix_of_top_k_plus_one() {
    // k is only a truncation length, so a smaller k must be a prefix.
    const SocialGraph g = random_graph(4242, 300, 3);
    for (const std::string& user : g.users()) {
        const auto five = recommend_fast(g, user, 5);
        auto three = recommend_fast(g, user, 3);
        std::vector<std::string> prefix(five.begin(),
                                        five.begin() + static_cast<long>(std::min<std::size_t>(3, five.size())));
        expect_eq_vec(three, prefix, "k=3 must be the prefix of k=5 for " + user);
    }
}

void test_large_sparse_graph_many_queries() {
    // 40k users, ~4 friends each, 2000 lookups.
    //
    // Scoring every user on every call is 2000 x 40k set intersections. Only
    // friends-of-friends can score at all, and there are ~16 of those.
    const SocialGraph g = random_graph(7, 40000, 2);
    const auto ids = g.users();
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < 2000 && i < ids.size(); ++i) {
        recommend_fast(g, ids[i], 5);
    }
    const double elapsed = seconds_since(start);
    expect(elapsed < 1.0, "2000 lookups took " + std::to_string(elapsed) + "s");
}

void test_high_degree_hub_is_handled() {
    // One celebrity with 8k friends: every fan shares exactly that one hop.
    //
    // Walking the hub's adjacency is unavoidable if the answer has to match the
    // baseline, so this pins correctness on the shape, not speed. Be ready to
    // say out loud when 2-hop expansion is the WRONG trade (see README).
    SocialGraph g;
    for (int i = 0; i < 8000; ++i) g.add_friendship("celebrity", "fan" + user_id(i));
    const auto recs = recommend_fast(g, "fan" + user_id(0), 3);
    expect_eq_vec(recs, {"fan" + user_id(1), "fan" + user_id(2), "fan" + user_id(3)},
                  "hub-neighbour ranking");
    expect_eq_vec(recs, recommend_baseline(g, "fan" + user_id(0), 3), "fast vs baseline on a hub");
    expect_eq_vec(recommend_fast(g, "celebrity", 3), {}, "the celebrity knows everyone already");
}

void run(const std::string& name, void (*fn)()) {
    try {
        fn();
        ++g_ok;
        std::cout << name << " ... ok\n";
    } catch (const AssertFail& e) {
        ++g_fail;
        std::cout << name << " ... FAIL\n  " << e.what() << "\n";
    } catch (const std::exception& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    }
}

}  // namespace

int main() {
    std::cout << "Phase1Validation\n";
    run("test_accepts_clean_candidate_list", test_accepts_clean_candidate_list);
    run("test_empty_candidate_list_is_valid", test_empty_candidate_list_is_valid);
    run("test_rejects_self_recommendation", test_rejects_self_recommendation);
    run("test_rejects_duplicates", test_rejects_duplicates);
    run("test_rejects_unknown_source_user", test_rejects_unknown_source_user);
    run("test_rejects_existing_friend", test_rejects_existing_friend);
    run("test_rejects_unknown_candidate", test_rejects_unknown_candidate);

    std::cout << "\nPhase2Baseline\n";
    run("test_ranks_by_mutual_friend_count", test_ranks_by_mutual_friend_count);
    run("test_excludes_self_and_existing_friends", test_excludes_self_and_existing_friends);
    run("test_ties_break_by_user_id", test_ties_break_by_user_id);
    run("test_respects_k", test_respects_k);
    run("test_unknown_user_returns_empty", test_unknown_user_returns_empty);
    run("test_no_mutual_friends_returns_empty", test_no_mutual_friends_returns_empty);
    run("test_output_passes_the_phase1_validator", test_output_passes_the_phase1_validator);

    std::cout << "\nPhase3Evaluate\n";
    run("test_perfect_recommender_scores_one", test_perfect_recommender_scores_one);
    run("test_precision_divides_by_k_not_by_len_recs", test_precision_divides_by_k_not_by_len_recs);
    run("test_recall_uses_holdout_size", test_recall_uses_holdout_size);
    run("test_metrics_average_over_users", test_metrics_average_over_users);
    run("test_empty_holdout_is_zero", test_empty_holdout_is_zero);
    run("test_non_positive_k_is_rejected", test_non_positive_k_is_rejected);

    std::cout << "\nPhase4Scale\n";
    run("test_same_answers_as_the_baseline", test_same_answers_as_the_baseline);
    run("test_fast_output_passes_the_validator", test_fast_output_passes_the_validator);
    run("test_top_k_is_a_prefix_of_top_k_plus_one", test_top_k_is_a_prefix_of_top_k_plus_one);
    run("test_large_sparse_graph_many_queries", test_large_sparse_graph_many_queries);
    run("test_high_degree_hub_is_handled", test_high_degree_hub_is_handled);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
