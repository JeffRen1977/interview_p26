// Staged spec for the Maximise-Unique-Characters task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "solver.h"
#include "wordlist.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <set>
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

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// --- helpers ---------------------------------------------------------------

// Exhaustive reference for small inputs only: 2^n subsets.
int brute_force(const std::vector<std::string>& words) {
    const std::vector<std::string> candidates = sanitize(words);
    const std::size_t n = candidates.size();
    if (n > 20) fail("brute_force is for small inputs only");
    int best = 0;
    for (unsigned long long subset = 0; subset < (1ULL << n); ++subset) {
        std::string joined;
        for (std::size_t i = 0; i < n; ++i) {
            if (subset & (1ULL << i)) joined += candidates[i];
        }
        const std::set<char> distinct(joined.begin(), joined.end());
        if (distinct.size() == joined.size()) {
            best = std::max(best, static_cast<int>(joined.size()));
        }
    }
    return best;
}

// Deterministic across implementations: mt19937 plus explicit modulo, never
// std::uniform_int_distribution (whose output is not portable).
std::vector<std::string> random_words(std::mt19937& rng, int count, int max_len = 4) {
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int length = static_cast<int>(rng() % static_cast<unsigned>(max_len)) + 1;
        std::string word;
        for (int j = 0; j < length; ++j) {
            word += static_cast<char>('a' + rng() % 8);
        }
        out.push_back(word);
    }
    return out;
}

std::vector<std::string> all_pairs(int alphabet_size) {
    std::vector<std::string> out;
    for (int i = 0; i < alphabet_size; ++i) {
        for (int j = i + 1; j < alphabet_size; ++j) {
            out.push_back(std::string(1, static_cast<char>('a' + i)) +
                          static_cast<char>('a' + j));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Phase 1 -- intake bugs
// ---------------------------------------------------------------------------
void test_trims_whitespace_from_both_ends() {
    // Stripping only the front leaves "\tXYZ\n" as "XYZ\n", which then fails
    // the a-z check and disappears entirely.
    expect_eq_vec(sanitize({"\tXYZ\n", "  cd  ", "ef"}), {"xyz", "cd", "ef"}, "trim both ends");
}

void test_lowercases_before_the_duplicate_check() {
    // "aA" lower-cases to "aa", which repeats a letter and must be dropped.
    expect_eq_vec(sanitize({"aA", "Ab", "cd"}), {"ab", "cd"}, "lower-case first");
}

void test_trims_lowercases_and_drops_invalid_words() {
    expect_eq_vec(sanitize({" Ab ", "aA", "b1b", "cd", "", "Hello", "\tXYZ\n"}),
                  {"ab", "cd", "xyz"}, "full intake pipeline");
}

void test_rejects_bytes_outside_ascii_a_to_z() {
    // In Python this needs an explicit isascii() check because str.isalpha() is
    // True for "cafe" with an accent. In C++ the byte-wise a-z test rejects any
    // multi-byte sequence for free -- keep the test as a contract anchor.
    expect_eq_vec(sanitize({"caf\xc3\xa9", "\xe6\x97\xa5\xe6\x9c\xac", "naive", "ok"}),
                  {"naive", "ok"}, "non-ASCII must be rejected");
}

void test_keeps_order_and_keeps_duplicate_candidates() {
    expect_eq_vec(sanitize({"dog", "cat", "dog"}), {"dog", "cat", "dog"}, "order and duplicates");
}

void test_empty_input() {
    expect(sanitize({}).empty(), "empty list");
    expect(sanitize({"", "  ", "123"}).empty(), "nothing usable");
}

void test_unique_char_count_counts_distinct_characters() {
    expect_eq_int(unique_char_count(""), 0, "\"\"");
    expect_eq_int(unique_char_count("abc"), 3, "\"abc\"");
    expect_eq_int(unique_char_count("aab"), 2, "\"aab\"");
    expect_eq_int(unique_char_count("aaaa"), 1, "\"aaaa\"");
}

// ---------------------------------------------------------------------------
// Phase 2 -- baseline backtracking
// ---------------------------------------------------------------------------
void test_classic_examples() {
    expect_eq_int(max_unique_length({"un", "iq", "ue"}), 4, "un/iq/ue");
    expect_eq_int(max_unique_length({"cha", "r", "act", "ers"}), 6, "cha/r/act/ers");
    expect_eq_int(max_unique_length({"abcdefghijklmnopqrstuvwxyz"}), 26, "whole alphabet");
}

void test_it_sanitizes_its_own_input() {
    expect_eq_int(max_unique_length({" UN ", "iq", "ue", "aa", "x1"}), 4, "raw input");
}

void test_no_usable_word() {
    expect_eq_int(max_unique_length({}), 0, "empty");
    expect_eq_int(max_unique_length({"aa", "bb", "123"}), 0, "nothing usable");
}

void test_taking_the_longest_word_first_is_not_optimal() {
    // greedy takes "abcd" (4); the optimum skips it for "ab" + "cef" (5)
    expect_eq_int(max_unique_length({"abcd", "ab", "cef"}), 5, "greedy is wrong");
    expect_eq_int(max_unique_length({"abcd", "efg"}), 7, "both fit");
}

void test_matches_brute_force_on_random_small_inputs() {
    std::mt19937 rng(17);
    for (int i = 0; i < 30; ++i) {
        const auto words = random_words(rng, static_cast<int>(rng() % 9));
        expect_eq_int(max_unique_length(words), brute_force(words),
                      "differential vs brute force, iteration " + std::to_string(i));
    }
}

// ---------------------------------------------------------------------------
// Phase 3 -- adversarial shapes
// ---------------------------------------------------------------------------
void test_same_answers_as_the_baseline() {
    expect_eq_int(max_unique_length_fast({"un", "iq", "ue"}), 4, "un/iq/ue");
    expect_eq_int(max_unique_length_fast({"cha", "r", "act", "ers"}), 6, "cha/r/act/ers");
    expect_eq_int(max_unique_length_fast({}), 0, "empty");
    expect_eq_int(max_unique_length_fast({"aa", "bb"}), 0, "nothing usable");
}

void test_differential_against_the_baseline() {
    std::mt19937 rng(23);
    for (int i = 0; i < 40; ++i) {
        const auto words = random_words(rng, static_cast<int>(rng() % 11));
        expect_eq_int(max_unique_length_fast(words), max_unique_length(words),
                      "fast vs baseline, iteration " + std::to_string(i));
    }
}

void test_helpers() {
    expect_eq_int(word_mask(""), 0, "word_mask(\"\")");
    expect_eq_int(word_mask("a"), 1, "word_mask(\"a\")");
    expect_eq_int(word_mask("ab"), 0b11, "word_mask(\"ab\")");
    expect_eq_int(popcount(word_mask("abcdef")), 6, "popcount");
}

void test_many_short_words() {
    // Every 2-letter word over a 12-letter alphabet: 66 candidates.
    // 2^66 subsets, but only 2^12 reachable letter sets. Anything that
    // enumerates subsets never returns.
    const auto words = all_pairs(12);
    expect_eq_int(static_cast<long long>(words.size()), 66, "candidate count");
    const auto start = std::chrono::steady_clock::now();
    const int best = max_unique_length_fast(words);
    const double elapsed = seconds_since(start);
    expect_eq_int(best, 12, "best over all 2-letter words");
    expect(elapsed < 2.0, "many-short-words took " + std::to_string(elapsed) + "s");
}

void test_a_few_very_long_words_buried_in_junk() {
    // 3,000 long words, almost all rejected because they repeat letters.
    std::string junk;
    for (int i = 0; i < 20; ++i) junk += "abcdefghij";
    std::vector<std::string> words(3000, junk);
    words.push_back("abcdefghijklm");
    words.push_back("nopqrstuvwxyz");

    const auto start = std::chrono::steady_clock::now();
    const int best = max_unique_length_fast(words);
    const double elapsed = seconds_since(start);
    expect_eq_int(best, 26, "two halves of the alphabet");
    expect(elapsed < 2.0, "long-words set took " + std::to_string(elapsed) + "s");
}

void test_duplicate_words_do_not_multiply_the_work() {
    std::vector<std::string> words(5000, "abc");
    words.insert(words.end(), 5000, "def");
    const auto start = std::chrono::steady_clock::now();
    expect_eq_int(max_unique_length_fast(words), 6, "abc + def");
    expect(seconds_since(start) < 2.0, "duplicate words took too long");
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
    std::cout << "Phase1Sanitize\n";
    run("test_trims_whitespace_from_both_ends", test_trims_whitespace_from_both_ends);
    run("test_lowercases_before_the_duplicate_check", test_lowercases_before_the_duplicate_check);
    run("test_trims_lowercases_and_drops_invalid_words", test_trims_lowercases_and_drops_invalid_words);
    run("test_rejects_bytes_outside_ascii_a_to_z", test_rejects_bytes_outside_ascii_a_to_z);
    run("test_keeps_order_and_keeps_duplicate_candidates", test_keeps_order_and_keeps_duplicate_candidates);
    run("test_empty_input", test_empty_input);
    run("test_unique_char_count_counts_distinct_characters", test_unique_char_count_counts_distinct_characters);

    std::cout << "\nPhase2Backtracking\n";
    run("test_classic_examples", test_classic_examples);
    run("test_it_sanitizes_its_own_input", test_it_sanitizes_its_own_input);
    run("test_no_usable_word", test_no_usable_word);
    run("test_taking_the_longest_word_first_is_not_optimal", test_taking_the_longest_word_first_is_not_optimal);
    run("test_matches_brute_force_on_random_small_inputs", test_matches_brute_force_on_random_small_inputs);

    std::cout << "\nPhase3Scale\n";
    run("test_same_answers_as_the_baseline", test_same_answers_as_the_baseline);
    run("test_differential_against_the_baseline", test_differential_against_the_baseline);
    run("test_helpers", test_helpers);
    run("test_many_short_words", test_many_short_words);
    run("test_a_few_very_long_words_buried_in_junk", test_a_few_very_long_words_buried_in_junk);
    run("test_duplicate_words_do_not_multiply_the_work", test_duplicate_words_do_not_multiply_the_work);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
