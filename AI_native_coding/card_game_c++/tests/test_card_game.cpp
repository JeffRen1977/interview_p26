// Staged spec for the Card Game task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "card.h"
#include "game_engine.h"
#include "rng.h"
#include "simulation.h"
#include "strategy.h"
#include "table.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
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

void expect_eq_int(long long got, long long want, const std::string& msg) {
    if (got != want) {
        fail(msg + " got=" + std::to_string(got) + " want=" + std::to_string(want));
    }
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// --- helpers ---------------------------------------------------------------

Card C(int rank, char suit = 'S') { return Card(rank, suit); }

std::vector<Card> as_vector(const Triplet& t) { return {t[0], t[1], t[2]}; }

// Exhaustive reference for small inputs only.
long long brute_force_count(const std::vector<Card>& cards) {
    const std::size_t n = cards.size();
    long long total = 0;
    for (std::size_t i = 0; i + 2 < n; ++i)
        for (std::size_t j = i + 1; j + 1 < n; ++j)
            for (std::size_t k = j + 1; k < n; ++k)
                if (cards[i].rank + cards[j].rank + cards[k].rank == kTargetSum) ++total;
    return total;
}

bool has_any_triplet(const std::vector<Card>& cards) { return brute_force_count(cards) > 0; }

std::vector<Card> random_cards(Rng& rng, int count) {
    std::vector<Card> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.emplace_back(rng.next_int(1, 9), kSuits[static_cast<std::size_t>(rng.next_int(0, 3))]);
    }
    return out;
}

std::map<Card, int> counts_of(const std::vector<Card>& cards) {
    std::map<Card, int> out;
    for (const Card& card : cards) ++out[card];
    return out;
}

// ---------------------------------------------------------------------------
// Phase 1 -- rule validation
// ---------------------------------------------------------------------------
Table base_table() {
    return Table({C(7, 'S'), C(3, 'H'), C(5, 'C'), C(9, 'D'), C(1, 'S')});
}

void test_legal_triplet_is_accepted() {
    Table table = base_table();
    GameEngine engine(table);
    expect(engine.is_valid_move({C(7, 'S'), C(3, 'H'), C(5, 'C')}), "7S+3H+5C should be legal");
}

void test_wrong_sum_is_rejected() {
    Table table = base_table();
    GameEngine engine(table);
    expect(!engine.is_valid_move({C(7, 'S'), C(3, 'H'), C(9, 'D')}), "sum is 19, not 15");
}

void test_wrong_size_is_rejected() {
    Table table = base_table();
    GameEngine engine(table);
    expect(!engine.is_valid_move({C(7, 'S'), C(3, 'H')}), "two cards");
    expect(!engine.is_valid_move({C(7, 'S'), C(3, 'H'), C(5, 'C'), C(1, 'S')}), "four cards");
}

void test_card_not_on_the_table_is_rejected() {
    // 7H has the same RANK as the 7S on the table, but it is a different card.
    Table table = base_table();
    GameEngine engine(table);
    expect(!engine.is_valid_move({C(7, 'H'), C(3, 'H'), C(5, 'C')}), "7H is not on the table");
}

void test_same_card_cannot_be_used_twice() {
    // Only one 5C is on the table, so 5C+5C+5C must not be legal.
    Table table = base_table();
    GameEngine engine(table);
    expect(!engine.is_valid_move({C(5, 'C'), C(5, 'C'), C(5, 'C')}), "one 5C consumed three times");
}

void test_duplicate_is_legal_when_the_table_really_holds_two_copies() {
    Table table({C(5, 'C'), C(5, 'C'), C(5, 'H')});
    GameEngine engine(table);
    expect(engine.is_valid_move({C(5, 'C'), C(5, 'C'), C(5, 'H')}), "two real copies of 5C");
}

void test_apply_move_scores_and_removes_exactly_those_cards() {
    Table table = base_table();
    GameEngine engine(table);
    engine.apply_move({C(7, 'S'), C(3, 'H'), C(5, 'C')});
    expect_eq_int(engine.score(), 1, "score");
    std::vector<Card> left = table.cards();
    std::sort(left.begin(), left.end());
    const std::vector<Card> want{C(1, 'S'), C(9, 'D')};
    expect(left == want, "wrong cards left on the table");
}

void test_apply_move_rejects_an_illegal_move() {
    Table table = base_table();
    GameEngine engine(table);
    bool threw = false;
    try {
        engine.apply_move({C(5, 'C'), C(5, 'C'), C(5, 'C')});
    } catch (const InvalidMove&) {
        threw = true;
    }
    expect(threw, "an illegal move must throw InvalidMove");
    expect_eq_int(engine.score(), 0, "score must not move");
    expect_eq_int(static_cast<long long>(table.size()), 5, "table must not change");
}

// ---------------------------------------------------------------------------
// Phase 2 -- baseline strategy and settlement loop
// ---------------------------------------------------------------------------
void test_choose_move_returns_a_legal_triplet() {
    GreedyStrategy strategy;
    Table table({C(9, 'S'), C(2, 'H'), C(4, 'C'), C(8, 'D')});
    const auto move = strategy.choose_move(table);
    expect(move.has_value(), "expected a move");
    GameEngine engine(table);
    expect(engine.is_valid_move(as_vector(*move)), "strategy returned an illegal move");
}

void test_choose_move_returns_none_when_no_set_exists() {
    GreedyStrategy strategy;
    Table table({C(9, 'S'), C(9, 'H'), C(9, 'C'), C(8, 'D')});
    expect(!strategy.choose_move(table).has_value(), "no triplet sums to 15 here");
}

void test_choose_move_on_empty_table() {
    GreedyStrategy strategy;
    Table table;
    expect(!strategy.choose_move(table).has_value(), "empty table");
}

void test_play_out_clears_one_set() {
    GreedyStrategy strategy;
    Table table({C(7, 'S'), C(3, 'H'), C(5, 'C'), C(9, 'D')});
    GameEngine engine(table);
    expect_eq_int(engine.play_out(strategy), 1, "score");
    const std::vector<Card> want{C(9, 'D')};
    expect(table.cards() == want, "only the 9D should remain");
}

void test_play_out_clears_two_disjoint_sets() {
    GreedyStrategy strategy;
    Table table({C(1, 'S'), C(6, 'H'), C(8, 'C'), C(2, 'D'), C(6, 'S'), C(7, 'H')});
    GameEngine engine(table);
    expect_eq_int(engine.play_out(strategy), 2, "score");
    expect_eq_int(static_cast<long long>(table.size()), 0, "table should be empty");
}

void test_play_out_scores_zero_when_nothing_is_playable() {
    GreedyStrategy strategy;
    Table table({C(9, 'S'), C(9, 'H'), C(1, 'C')});
    GameEngine engine(table);
    expect_eq_int(engine.play_out(strategy), 0, "score");
    expect_eq_int(static_cast<long long>(table.size()), 3, "table untouched");
}

void test_play_out_respects_max_moves() {
    GreedyStrategy strategy;
    Table table({C(5, 'S'), C(5, 'H'), C(5, 'C'), C(5, 'D'), C(5, 'S'), C(5, 'H')});
    GameEngine engine(table);
    expect_eq_int(engine.play_out(strategy, 1), 1, "score capped at 1");
    expect_eq_int(static_cast<long long>(table.size()), 3, "only one set removed");
}

void test_play_out_conserves_cards_and_reaches_a_terminal_state() {
    GreedyStrategy strategy;
    Rng rng(7);
    for (int iteration = 0; iteration < 20; ++iteration) {
        const std::vector<Card> cards = random_cards(rng, 14);
        Table table(cards);
        GameEngine engine(table);
        const int score = engine.play_out(strategy);
        const std::vector<Card> remaining = table.cards();

        // no card was invented, duplicated or lost
        expect_eq_int(static_cast<long long>(remaining.size()),
                      static_cast<long long>(cards.size()) - kSetSize * score,
                      "card count after play_out");
        const std::map<Card, int> before = counts_of(cards);
        const std::map<Card, int> after = counts_of(remaining);
        long long consumed = 0;
        for (const auto& [card, n] : before) {
            const auto it = after.find(card);
            const int left = it == after.end() ? 0 : it->second;
            expect(left <= n, "a card was invented: " + card.str());
            consumed += n - left;
        }
        expect_eq_int(consumed, kSetSize * score, "consumed cards must equal 3 per move");
        expect(!has_any_triplet(remaining), "strategy stopped with a set still playable");
    }
}

// ---------------------------------------------------------------------------
// Phase 3 -- scale, adversarial input and Monte-Carlo simulation
// ---------------------------------------------------------------------------
void test_rank_triples_helper() {
    const auto triples = all_triplet_ranks();
    expect_eq_int(static_cast<long long>(triples.size()), 13, "there are exactly 13");
    bool has_159 = false, has_555 = false;
    for (const auto& t : triples) {
        expect_eq_int(t[0] + t[1] + t[2], kTargetSum, "every triple sums to 15");
        expect(t[0] <= t[1] && t[1] <= t[2], "non-decreasing");
        if (t == std::array<int, 3>{1, 5, 9}) has_159 = true;
        if (t == std::array<int, 3>{5, 5, 5}) has_555 = true;
    }
    expect(has_159 && has_555, "(1,5,9) and (5,5,5) must be there");
}

void test_count_matches_brute_force_on_random_tables() {
    Rng rng(11);
    for (int i = 0; i < 25; ++i) {
        const auto cards = random_cards(rng, rng.next_int(0, 18));
        expect_eq_int(count_triplets_sum_15(cards), brute_force_count(cards),
                      "differential vs brute force, iteration " + std::to_string(i));
    }
}

void test_count_on_edge_inputs() {
    expect_eq_int(count_triplets_sum_15({}), 0, "empty");
    expect_eq_int(count_triplets_sum_15({C(5), C(5)}), 0, "two cards");
    expect_eq_int(count_triplets_sum_15({C(5), C(5), C(5)}), 1, "exactly one");
    expect_eq_int(count_triplets_sum_15(std::vector<Card>(6, C(5))), 20, "C(6,3)");
    expect_eq_int(count_triplets_sum_15(std::vector<Card>(100, C(9))), 0, "27 != 15");
}

void test_count_stays_in_64_bit() {
    // C(6000, 3) = 35,982,002,000 -- past int32. So is the numerator
    // 6000*5999*5998 = 215,892,012,000. Anything computed in int overflows,
    // and signed overflow is UB, not a wrap you can rely on.
    const std::vector<Card> cards(6000, C(5));
    expect_eq_int(count_triplets_sum_15(cards), 35982002000LL, "C(6000,3)");
}

void test_count_is_fast_on_a_huge_table() {
    Rng rng(3);
    const auto cards = random_cards(rng, 20000);
    const auto start = std::chrono::steady_clock::now();
    const long long total = count_triplets_sum_15(cards);
    const double elapsed = seconds_since(start);
    expect(total > 0, "a random 20k table has plenty of triplets");
    expect(elapsed < 0.5, "count_triplets_sum_15 took " + std::to_string(elapsed) + "s on 20k cards");
}

void test_fast_strategy_picks_legal_moves() {
    Rng rng(5);
    FastStrategy strategy;
    for (int i = 0; i < 20; ++i) {
        const auto cards = random_cards(rng, 12);
        Table table(cards);
        const auto move = strategy.choose_move(table);
        if (!move.has_value()) {
            expect(!has_any_triplet(cards), "returned nullopt while a triplet existed");
        } else {
            GameEngine engine(table);
            expect(engine.is_valid_move(as_vector(*move)), "returned an illegal move");
        }
    }
}

void test_fast_strategy_reaches_a_terminal_state() {
    Rng rng(13);
    FastStrategy strategy;
    for (int i = 0; i < 20; ++i) {
        Table table(random_cards(rng, 14));
        GameEngine engine(table);
        engine.play_out(strategy);
        expect(!has_any_triplet(table.cards()), "stopped with a set still playable");
    }
}

void test_fast_strategy_survives_the_adversarial_table() {
    // 2,000 nines (no triplet among them) hiding nine fives at the end.
    // Brute force has to enumerate C(2009, 3) combinations to prove the nines
    // are dead, which is why Phase 2's strategy times out here.
    std::vector<Card> cards(2000, C(9, 'S'));
    cards.insert(cards.end(), 9, C(5, 'H'));
    Table table(cards);
    GameEngine engine(table);
    FastStrategy strategy;

    const auto start = std::chrono::steady_clock::now();
    const int score = engine.play_out(strategy);
    const double elapsed = seconds_since(start);
    expect_eq_int(score, 3, "three (5,5,5) sets");
    expect_eq_int(static_cast<long long>(table.size()), 2000, "the nines stay");
    expect(elapsed < 2.0, "play_out took " + std::to_string(elapsed) + "s on a 2k table");
}

void test_monte_carlo_is_deterministic_for_a_seed() {
    FastStrategy strategy;
    const MonteCarloStats a = monte_carlo(strategy, 40, 20, 42);
    const MonteCarloStats b = monte_carlo(strategy, 40, 20, 42);
    expect(a == b, "same seed must give the same stats");
    expect_eq_int(a.games, 40, "games");
    expect(a.total_score > 0, "40 tables of 20 cards should score something");
    expect(std::fabs(a.mean_score - static_cast<double>(a.total_score) / 40.0) < 1e-9, "mean");
    expect(a.max_score >= a.mean_score, "max >= mean");
}

void test_monte_carlo_handles_a_table_too_small_to_score() {
    FastStrategy strategy;
    const MonteCarloStats stats = monte_carlo(strategy, 5, 2, 1);
    expect_eq_int(stats.total_score, 0, "two cards can never make a triplet");
    expect_eq_int(stats.zero_score_games, 5, "every game scores zero");
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
    std::cout << "Phase1RuleValidation\n";
    run("test_legal_triplet_is_accepted", test_legal_triplet_is_accepted);
    run("test_wrong_sum_is_rejected", test_wrong_sum_is_rejected);
    run("test_wrong_size_is_rejected", test_wrong_size_is_rejected);
    run("test_card_not_on_the_table_is_rejected", test_card_not_on_the_table_is_rejected);
    run("test_same_card_cannot_be_used_twice", test_same_card_cannot_be_used_twice);
    run("test_duplicate_is_legal_when_the_table_really_holds_two_copies", test_duplicate_is_legal_when_the_table_really_holds_two_copies);
    run("test_apply_move_scores_and_removes_exactly_those_cards", test_apply_move_scores_and_removes_exactly_those_cards);
    run("test_apply_move_rejects_an_illegal_move", test_apply_move_rejects_an_illegal_move);

    std::cout << "\nPhase2GreedyStrategy\n";
    run("test_choose_move_returns_a_legal_triplet", test_choose_move_returns_a_legal_triplet);
    run("test_choose_move_returns_none_when_no_set_exists", test_choose_move_returns_none_when_no_set_exists);
    run("test_choose_move_on_empty_table", test_choose_move_on_empty_table);
    run("test_play_out_clears_one_set", test_play_out_clears_one_set);
    run("test_play_out_clears_two_disjoint_sets", test_play_out_clears_two_disjoint_sets);
    run("test_play_out_scores_zero_when_nothing_is_playable", test_play_out_scores_zero_when_nothing_is_playable);
    run("test_play_out_respects_max_moves", test_play_out_respects_max_moves);
    run("test_play_out_conserves_cards_and_reaches_a_terminal_state", test_play_out_conserves_cards_and_reaches_a_terminal_state);

    std::cout << "\nPhase3ScaleAndEdges\n";
    run("test_rank_triples_helper", test_rank_triples_helper);
    run("test_count_matches_brute_force_on_random_tables", test_count_matches_brute_force_on_random_tables);
    run("test_count_on_edge_inputs", test_count_on_edge_inputs);
    run("test_count_stays_in_64_bit", test_count_stays_in_64_bit);
    run("test_count_is_fast_on_a_huge_table", test_count_is_fast_on_a_huge_table);
    run("test_fast_strategy_picks_legal_moves", test_fast_strategy_picks_legal_moves);
    run("test_fast_strategy_reaches_a_terminal_state", test_fast_strategy_reaches_a_terminal_state);
    run("test_fast_strategy_survives_the_adversarial_table", test_fast_strategy_survives_the_adversarial_table);
    run("test_monte_carlo_is_deterministic_for_a_seed", test_monte_carlo_is_deterministic_for_a_seed);
    run("test_monte_carlo_handles_a_table_too_small_to_score", test_monte_carlo_handles_a_table_too_small_to_score);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
