// Staged spec for the TTL KV + tag index task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "clock.h"
#include "store.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kNow = 1000.0;

int g_ok = 0;
int g_fail = 0;
int g_error = 0;

struct AssertFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void fail(const std::string& msg) { throw AssertFail(msg); }

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        fail(msg);
    }
}

void expect_eq_str(const std::optional<std::string>& got, const std::string& want, const std::string& msg) {
    if (!got.has_value() || *got != want) {
        fail(msg + " got=" + (got ? *got : "nullopt") + " want=" + want);
    }
}

void expect_none(const std::optional<std::string>& got, const std::string& msg) {
    if (got.has_value()) {
        fail(msg + " got=" + *got);
    }
}

void expect_eq_vec(const std::vector<std::string>& got,
                   const std::vector<std::string>& want,
                   const std::string& msg) {
    if (got != want) {
        std::string detail = msg + " got=[";
        for (size_t i = 0; i < got.size(); ++i) {
            if (i) detail += ", ";
            detail += got[i];
        }
        detail += "] want=[";
        for (size_t i = 0; i < want.size(); ++i) {
            if (i) detail += ", ";
            detail += want[i];
        }
        detail += "]";
        fail(detail);
    }
}

void run(const std::string& name, void (*fn)()) {
    try {
        fn();
        ++g_ok;
        std::cout << name << " ... ok\n";
    } catch (const AssertFail& e) {
        ++g_fail;
        std::cout << name << " ... FAIL\n  " << e.what() << "\n";
    } catch (const std::logic_error& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    } catch (const std::exception& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    }
}

// ----------------------------------------------------------------------
// Phase 1 — TTL bugs
// ----------------------------------------------------------------------

void test_set_get_roundtrip() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1");
    expect_eq_str(store.get("a"), "1", "get");
    expect(store.exists("a"), "exists");
    expect(store.size() == 1, "size");
}

void test_missing_key_is_none() {
    FakeClock clock(kNow);
    Store store(clock);
    expect_none(store.get("missing"), "get missing");
    expect(!store.exists("missing"), "exists missing");
    expect(!store.erase("missing"), "erase missing");
}

void test_overwrite_replaces_value() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1");
    store.set("a", "2");
    expect_eq_str(store.get("a"), "2", "get");
    expect(store.size() == 1, "size");
}

void test_delete_removes_key() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1");
    expect(store.erase("a"), "erase");
    expect_none(store.get("a"), "get after erase");
    expect(store.size() == 0, "size");
}

void test_no_ttl_never_expires() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1");
    clock.advance(10000);
    expect_eq_str(store.get("a"), "1", "get");
    expect(store.exists("a"), "exists");
}

void test_exists_and_size_stay_live_before_ttl() {
    // ttl is seconds-from-now, not an absolute timestamp.
    // Clock starts at 1000. set(..., ttl=10) must still be live at t=1009.
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1", 10.0);
    expect(store.exists("a"), "exists at t=1000");
    expect(store.size() == 1, "size at t=1000");
    clock.advance(9);
    expect(store.exists("a"), "exists at t=1009");
    expect(store.size() == 1, "size at t=1009");
}

void test_get_returns_none_once_ttl_elapses() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1", 10.0);
    clock.advance(10);
    expect_none(store.get("a"), "get after ttl");
    expect(!store.exists("a"), "exists after ttl");
    expect(store.size() == 0, "size after ttl");
}

void test_expired_at_the_exact_boundary() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1", 5.0);
    clock.advance(5);
    expect_none(store.get("a"), "get at boundary");
}

void test_overwrite_resets_ttl() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1", 5.0);
    clock.advance(4);
    store.set("a", "2", 10.0);
    clock.advance(5);
    expect_eq_str(store.get("a"), "2", "still live after reset");
    clock.advance(5);
    expect_none(store.get("a"), "dead after new ttl");
}

// ----------------------------------------------------------------------
// Phase 2 — tag query (scan)
// ----------------------------------------------------------------------

void seed_colors(Store& store) {
    store.set("red", "1", std::nullopt, {"color", "warm"});
    store.set("blue", "2", std::nullopt, {"color", "cool"});
    store.set("sun", "3", std::nullopt, {"warm"});
    store.set("plain", "4");
}

void test_match_all_is_and() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    expect_eq_vec(store.query({"color", "warm"}, "all"), {"red"}, "color AND warm");
    expect_eq_vec(store.query({"color"}, "all"), {"blue", "red"}, "color");
}

void test_match_any_is_or() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    expect_eq_vec(store.query({"warm", "cool"}, "any"), {"blue", "red", "sun"}, "warm OR cool");
}

void test_empty_tags() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    expect_eq_vec(store.query({}, "all"), {"blue", "plain", "red", "sun"}, "empty AND");
    expect_eq_vec(store.query({}, "any"), {}, "empty OR");
}

void test_unknown_tag_is_empty() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    expect_eq_vec(store.query({"nope"}, "all"), {}, "unknown all");
    expect_eq_vec(store.query({"nope"}, "any"), {}, "unknown any");
}

void test_query_skips_expired_keys() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    store.set("hot", "9", 2.0, {"warm"});
    expect_eq_vec(store.query({"warm"}, "all"), {"hot", "red", "sun"}, "before expire");
    clock.advance(2);
    expect_eq_vec(store.query({"warm"}, "all"), {"red", "sun"}, "after expire");
}

void test_overwrite_replaces_tags() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    store.set("red", "1", std::nullopt, {"cool"});
    expect_eq_vec(store.query({"warm"}, "all"), {"sun"}, "warm after overwrite");
    expect_eq_vec(store.query({"cool"}, "all"), {"blue", "red"}, "cool after overwrite");
}

void test_delete_drops_key_from_query() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    store.erase("red");
    expect_eq_vec(store.query({"color"}, "all"), {"blue"}, "color after erase");
}

// ----------------------------------------------------------------------
// Phase 3 — inverted index
// ----------------------------------------------------------------------

void test_same_answers_as_the_baseline() {
    FakeClock clock(kNow);
    Store store(clock);
    seed_colors(store);
    expect_eq_vec(store.query_fast({"color", "warm"}, "all"),
                  store.query({"color", "warm"}, "all"), "all differential");
    expect_eq_vec(store.query_fast({"warm", "cool"}, "any"),
                  store.query({"warm", "cool"}, "any"), "any differential");
    expect_eq_vec(store.query_fast({}, "all"), store.query({}, "all"), "empty all");
    expect_eq_vec(store.query_fast({}, "any"), store.query({}, "any"), "empty any");
}

void test_overwrite_and_expire_keep_the_index_honest() {
    FakeClock clock(kNow);
    Store store(clock);
    store.set("a", "1", std::nullopt, {"hot", "keep"});
    store.set("b", "2", 3.0, {"hot"});
    store.set("a", "3", std::nullopt, {"cold"});
    expect_eq_vec(store.query_fast({"hot"}, "all"), {"b"}, "hot after overwrite");
    expect_eq_vec(store.query_fast({"cold"}, "all"), {"a"}, "cold after overwrite");
    clock.advance(3);
    expect_eq_vec(store.query_fast({"hot"}, "all"), {}, "hot after expire");
    expect_eq_vec(store.query_fast({"cold"}, "all"), {"a"}, "cold still live");
}

void test_repeated_rare_tag_lookups() {
    // 40k keys, a tag that hits two of them, queried 3000 times.
    // Scanning every key on each lookup is ~120M visits and misses the budget.
    FakeClock clock(kNow);
    Store store(clock);
    for (int i = 0; i < 40000; ++i) {
        store.set("k" + std::to_string(i), std::to_string(i), std::nullopt,
                  {"bucket-" + std::to_string(i % 50), "common"});
    }
    store.set("needle-a", "a", std::nullopt, {"rare", "common"});
    store.set("needle-b", "b", std::nullopt, {"rare", "other"});

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 3000; ++i) {
        expect_eq_vec(store.query_fast({"rare"}, "all"), {"needle-a", "needle-b"}, "rare lookup");
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    expect(elapsed < 1.0, "rare-tag loop took " + std::to_string(elapsed) + "s");
}

void test_multi_tag_intersection_is_from_posting_lists() {
    FakeClock clock(kNow);
    Store store(clock);
    for (int i = 0; i < 8000; ++i) {
        store.set("only-a-" + std::to_string(i), std::to_string(i), std::nullopt, {"a"});
        store.set("only-b-" + std::to_string(i), std::to_string(i), std::nullopt, {"b"});
    }
    store.set("both", "0", std::nullopt, {"a", "b", "c"});
    const auto start = std::chrono::steady_clock::now();
    expect_eq_vec(store.query_fast({"a", "b"}, "all"), {"both"}, "intersection");
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    expect(elapsed < 0.5, "intersection took " + std::to_string(elapsed) + "s");
}

}  // namespace

int main() {
    std::cout << "Phase1TTL\n";
    run("test_set_get_roundtrip", test_set_get_roundtrip);
    run("test_missing_key_is_none", test_missing_key_is_none);
    run("test_overwrite_replaces_value", test_overwrite_replaces_value);
    run("test_delete_removes_key", test_delete_removes_key);
    run("test_no_ttl_never_expires", test_no_ttl_never_expires);
    run("test_exists_and_size_stay_live_before_ttl", test_exists_and_size_stay_live_before_ttl);
    run("test_get_returns_none_once_ttl_elapses", test_get_returns_none_once_ttl_elapses);
    run("test_expired_at_the_exact_boundary", test_expired_at_the_exact_boundary);
    run("test_overwrite_resets_ttl", test_overwrite_resets_ttl);

    std::cout << "\nPhase2TagQuery\n";
    run("test_match_all_is_and", test_match_all_is_and);
    run("test_match_any_is_or", test_match_any_is_or);
    run("test_empty_tags", test_empty_tags);
    run("test_unknown_tag_is_empty", test_unknown_tag_is_empty);
    run("test_query_skips_expired_keys", test_query_skips_expired_keys);
    run("test_overwrite_replaces_tags", test_overwrite_replaces_tags);
    run("test_delete_drops_key_from_query", test_delete_drops_key_from_query);

    std::cout << "\nPhase3Scale\n";
    run("test_same_answers_as_the_baseline", test_same_answers_as_the_baseline);
    run("test_overwrite_and_expire_keep_the_index_honest", test_overwrite_and_expire_keep_the_index_honest);
    run("test_repeated_rare_tag_lookups", test_repeated_rare_tag_lookups);
    run("test_multi_tag_intersection_is_from_posting_lists", test_multi_tag_intersection_is_from_posting_lists);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
