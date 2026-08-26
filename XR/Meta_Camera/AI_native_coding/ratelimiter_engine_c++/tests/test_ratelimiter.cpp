// Staged spec for the RateLimiter engine. This file is the contract.
//
//     make test      # build and run
//
// Unlike the other samples there is no project/ vs solution/ split: project/
// holds a complete reference engine. To drill it, gut RateLimiter::allow and
// rebuild it one stage at a time (see README §2).

#include "memory_store.h"
#include "ratelimiter.h"
#include "request.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
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

void expect_decision(Decision got, Decision want, const std::string& msg) {
    if (got != want) {
        fail(msg + " got=" + to_string(got) + " want=" + to_string(want));
    }
}

void expect_eq_int(long long got, long long want, const std::string& msg) {
    if (got != want) fail(msg + " got=" + std::to_string(got) + " want=" + std::to_string(want));
}

Request req(const std::string& user = "u1",
            const std::string& endpoint = "/api",
            long long ts = 0,
            int cost = 1) {
    return Request{user, endpoint, ts, cost};
}

// C++17 has no std::barrier (that arrived in C++20), so here is the three-line
// version. Every worker must be parked before any of them starts hammering the
// limiter, otherwise the test measures start-up skew instead of contention.
class Barrier {
public:
    explicit Barrier(int count) : remaining_(count), total_(count) {}
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        const int generation = generation_;
        if (--remaining_ == 0) {
            remaining_ = total_;
            ++generation_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [&] { return generation_ != generation; });
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int remaining_;
    int total_;
    int generation_ = 0;
};

// ---------------------------------------------------------------------------
// Stage 1 -- fixed default rule: 10 units / 1000ms, no soft-degrade band
// ---------------------------------------------------------------------------
void test_first_request_allowed() {
    MemoryStore store;
    RateLimiter limiter(store);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Allow, "first request");
}

void test_burst_up_to_limit_then_deny() {
    MemoryStore store;
    RateLimiter limiter(store);
    for (int i = 0; i < 10; ++i) {
        expect_decision(limiter.allow(req("u1", "/api", i)), Decision::Allow,
                        "request " + std::to_string(i));
    }
    expect_decision(limiter.allow(req("u1", "/api", 10)), Decision::Deny, "the 11th");
}

void test_denied_request_does_not_consume_quota() {
    MemoryStore store;
    RateLimiter limiter(store);
    for (int i = 0; i < 10; ++i) limiter.allow(req("u1", "/api", 0));
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Deny, "first denial");
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Deny, "second denial");
}

// ---------------------------------------------------------------------------
// Stage 2 -- tenant and endpoint isolation
// ---------------------------------------------------------------------------
void test_tenants_are_isolated() {
    MemoryStore store;
    RateLimiter limiter(store, 2, 1000);
    expect_decision(limiter.allow(req("a", "/api", 0)), Decision::Allow, "a #1");
    expect_decision(limiter.allow(req("a", "/api", 1)), Decision::Allow, "a #2");
    expect_decision(limiter.allow(req("a", "/api", 2)), Decision::Deny, "a #3");
    expect_decision(limiter.allow(req("b", "/api", 2)), Decision::Allow, "b is unaffected");
}

void test_endpoints_are_isolated() {
    MemoryStore store;
    RateLimiter limiter(store, 2, 1000);
    expect_decision(limiter.allow(req("u1", "/search", 0)), Decision::Allow, "/search #1");
    expect_decision(limiter.allow(req("u1", "/search", 1)), Decision::Allow, "/search #2");
    expect_decision(limiter.allow(req("u1", "/search", 2)), Decision::Deny, "/search #3");
    expect_decision(limiter.allow(req("u1", "/upload", 2)), Decision::Allow, "/upload is unaffected");
}

// ---------------------------------------------------------------------------
// Stage 3 -- cost-weighted quota
// ---------------------------------------------------------------------------
void test_cost_consumes_multiple_units() {
    MemoryStore store;
    RateLimiter limiter(store, 5, 1000);
    expect_decision(limiter.allow(req("u1", "/api", 0, 3)), Decision::Allow, "cost 3");
    expect_decision(limiter.allow(req("u1", "/api", 1, 2)), Decision::Allow, "cost 2");
    expect_decision(limiter.allow(req("u1", "/api", 2, 1)), Decision::Deny, "over budget");
}

void test_single_request_heavier_than_limit_is_denied() {
    MemoryStore store;
    RateLimiter limiter(store, 5, 1000);
    expect_decision(limiter.allow(req("u1", "/api", 0, 6)), Decision::Deny, "cost 6 > limit 5");
}

void test_zero_cost_does_not_consume() {
    MemoryStore store;
    RateLimiter limiter(store, 5, 1000);
    for (int i = 0; i < 5; ++i) limiter.allow(req("u1", "/api", 0, 1));
    expect_decision(limiter.allow(req("u1", "/api", 0, 0)), Decision::Allow, "free request");
}

void test_negative_cost_is_rejected() {
    MemoryStore store;
    RateLimiter limiter(store);
    bool threw = false;
    try {
        limiter.allow(req("u1", "/api", 0, -1));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "a negative cost must throw std::invalid_argument");
}

// ---------------------------------------------------------------------------
// Stage 4 -- sliding window driven by injected timestamps
// ---------------------------------------------------------------------------
void test_oldest_event_expires_exactly_at_window_boundary() {
    MemoryStore store;
    RateLimiter limiter(store, 2, 1000);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Allow, "t=0");
    expect_decision(limiter.allow(req("u1", "/api", 500)), Decision::Allow, "t=500");
    expect_decision(limiter.allow(req("u1", "/api", 999)), Decision::Deny, "t=999 still full");
    // the event at t=0 is gone once now - window_ms == 0 (strictly older)
    expect_decision(limiter.allow(req("u1", "/api", 1000)), Decision::Allow, "t=1000 frees a slot");
}

void test_does_not_use_wall_clock() {
    MemoryStore store;
    RateLimiter limiter(store, 2, 1000);
    limiter.allow(req("u1", "/api", 10000));
    limiter.allow(req("u1", "/api", 10001));
    expect_decision(limiter.allow(req("u1", "/api", 10002)), Decision::Deny,
                    "timestamps come from the request, never from a clock");
}

// ---------------------------------------------------------------------------
// Stage 5 -- hot-reloadable per-endpoint rules
// ---------------------------------------------------------------------------
void test_per_endpoint_rule_overrides_default() {
    MemoryStore store;
    RateLimiter limiter(store, 100, 1000);
    limiter.set_rule("/search", 1, 1000);
    expect_decision(limiter.allow(req("u1", "/search", 0)), Decision::Allow, "/search #1");
    expect_decision(limiter.allow(req("u1", "/search", 1)), Decision::Deny, "/search #2");
    expect_decision(limiter.allow(req("u1", "/other", 1)), Decision::Allow, "/other uses the default");
}

void test_hot_reload_tighter_limit() {
    MemoryStore store;
    RateLimiter limiter(store, 100, 1000);
    limiter.set_rule("/api", 3, 1000);
    limiter.allow(req("u1", "/api", 0));
    limiter.allow(req("u1", "/api", 1));
    limiter.set_rule("/api", 2, 1000);
    expect_decision(limiter.allow(req("u1", "/api", 2)), Decision::Deny, "new limit applies at once");
}

void test_hot_reload_shorter_window_drops_old_events() {
    MemoryStore store;
    RateLimiter limiter(store, 100, 1000);
    limiter.set_rule("/api", 1, 5000);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Allow, "t=0");
    limiter.set_rule("/api", 1, 100);
    // t=0 is outside the new 100ms window at t=200
    expect_decision(limiter.allow(req("u1", "/api", 200)), Decision::Allow, "shorter window frees it");
}

void test_invalid_rules_are_rejected() {
    MemoryStore store;
    RateLimiter limiter(store);
    int threw = 0;
    try { limiter.set_rule("/api", -1, 1000); } catch (const std::invalid_argument&) { ++threw; }
    try { limiter.set_rule("/api", 5, 0); } catch (const std::invalid_argument&) { ++threw; }
    try { limiter.set_rule("/api", 5, 1000, 6); } catch (const std::invalid_argument&) { ++threw; }
    expect_eq_int(threw, 3, "negative limit, zero window and threshold > limit must all throw");
}

// ---------------------------------------------------------------------------
// Stage 6 -- soft degrade and store failure
// ---------------------------------------------------------------------------
void test_soft_limit_returns_degrade_and_still_consumes() {
    MemoryStore store;
    RateLimiter limiter(store, 5, 1000);
    limiter.set_rule("/api", 5, 1000, 3);
    expect_decision(limiter.allow(req("u1", "/api", 0, 3)), Decision::Allow, "at the threshold");
    expect_decision(limiter.allow(req("u1", "/api", 1, 1)), Decision::Degrade, "over the soft band");
    expect_decision(limiter.allow(req("u1", "/api", 2, 1)), Decision::Degrade, "still under the limit");
    expect_decision(limiter.allow(req("u1", "/api", 3, 1)), Decision::Deny, "over the hard limit");
}

void test_store_failure_degrades_instead_of_raising() {
    MemoryStore store;
    RateLimiter limiter(store, 1, 1000);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Allow, "warm-up");
    store.set_failing(true);
    expect_decision(limiter.allow(req("u1", "/api", 1)), Decision::Degrade,
                    "the gateway stays up when the store is down");
}

void test_store_failure_on_first_request_still_degrades() {
    MemoryStore store;
    store.set_failing(true);
    RateLimiter limiter(store);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Degrade, "cold failure");
}

void test_store_recovers() {
    MemoryStore store;
    RateLimiter limiter(store, 1, 1000);
    store.set_failing(true);
    expect_decision(limiter.allow(req("u1", "/api", 0)), Decision::Degrade, "while down");
    store.set_failing(false);
    expect_decision(limiter.allow(req("u1", "/api", 1)), Decision::Allow,
                    "a degraded request must not have consumed quota");
}

// ---------------------------------------------------------------------------
// Stage 7 -- thread safety
// ---------------------------------------------------------------------------
void test_same_tenant_never_oversells_under_contention() {
    MemoryStore store;
    RateLimiter limiter(store, 50, 10000);
    const int n_threads = 16;
    const int per_thread = 20;
    Barrier barrier(n_threads);
    std::mutex tally_lock;
    int accepted = 0;

    std::vector<std::thread> threads;
    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([&] {
            barrier.wait();
            int local = 0;
            for (int i = 0; i < per_thread; ++i) {
                if (limiter.allow(req("acme", "/api", i)) != Decision::Deny) ++local;
            }
            std::lock_guard<std::mutex> guard(tally_lock);
            accepted += local;
        });
    }
    for (auto& thread : threads) thread.join();
    expect_eq_int(accepted, 50, "exactly the limit, no oversell and no undersell");
}

void test_tenants_keep_independent_quota_under_contention() {
    MemoryStore store;
    RateLimiter limiter(store, 10, 10000);
    const std::vector<std::string> tenants{"acme", "globex"};
    const int n_threads = 8;
    const int per_thread = 10;
    Barrier barrier(static_cast<int>(tenants.size()) * n_threads);
    std::mutex tally_lock;
    std::vector<int> accepted(tenants.size(), 0);

    std::vector<std::thread> threads;
    for (std::size_t ti = 0; ti < tenants.size(); ++ti) {
        for (int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&, ti] {
                barrier.wait();
                int local = 0;
                for (int i = 0; i < per_thread; ++i) {
                    if (limiter.allow(req(tenants[ti], "/api", i)) != Decision::Deny) ++local;
                }
                std::lock_guard<std::mutex> guard(tally_lock);
                accepted[ti] += local;
            });
        }
    }
    for (auto& thread : threads) thread.join();
    expect_eq_int(accepted[0], 10, "acme");
    expect_eq_int(accepted[1], 10, "globex");
}

void test_hot_reload_races_with_traffic() {
    // set_rule and allow run concurrently. Nothing here checks a quota number --
    // the point is that neither a data race nor a torn Rule read shows up under
    // -fsanitize=thread. Run it that way at least once.
    MemoryStore store;
    RateLimiter limiter(store, 100, 10000);
    Barrier barrier(5);
    std::vector<std::thread> threads;

    threads.emplace_back([&] {
        barrier.wait();
        for (int i = 0; i < 200; ++i) {
            limiter.set_rule("/api", 1 + (i % 50), 1000 + i, (i % 2) ? std::optional<int>{} : std::optional<int>{1});
        }
    });
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            barrier.wait();
            for (int i = 0; i < 200; ++i) {
                limiter.allow(req("tenant" + std::to_string(t), "/api", i));
            }
        });
    }
    for (auto& thread : threads) thread.join();
    expect(true, "no crash, no torn read");
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
    std::cout << "Stage1DefaultWindow\n";
    run("test_first_request_allowed", test_first_request_allowed);
    run("test_burst_up_to_limit_then_deny", test_burst_up_to_limit_then_deny);
    run("test_denied_request_does_not_consume_quota", test_denied_request_does_not_consume_quota);

    std::cout << "\nStage2MultiTenantAndEndpoint\n";
    run("test_tenants_are_isolated", test_tenants_are_isolated);
    run("test_endpoints_are_isolated", test_endpoints_are_isolated);

    std::cout << "\nStage3CostWeighted\n";
    run("test_cost_consumes_multiple_units", test_cost_consumes_multiple_units);
    run("test_single_request_heavier_than_limit_is_denied", test_single_request_heavier_than_limit_is_denied);
    run("test_zero_cost_does_not_consume", test_zero_cost_does_not_consume);
    run("test_negative_cost_is_rejected", test_negative_cost_is_rejected);

    std::cout << "\nStage4SlidingWindow\n";
    run("test_oldest_event_expires_exactly_at_window_boundary", test_oldest_event_expires_exactly_at_window_boundary);
    run("test_does_not_use_wall_clock", test_does_not_use_wall_clock);

    std::cout << "\nStage5DynamicRules\n";
    run("test_per_endpoint_rule_overrides_default", test_per_endpoint_rule_overrides_default);
    run("test_hot_reload_tighter_limit", test_hot_reload_tighter_limit);
    run("test_hot_reload_shorter_window_drops_old_events", test_hot_reload_shorter_window_drops_old_events);
    run("test_invalid_rules_are_rejected", test_invalid_rules_are_rejected);

    std::cout << "\nStage6Degrade\n";
    run("test_soft_limit_returns_degrade_and_still_consumes", test_soft_limit_returns_degrade_and_still_consumes);
    run("test_store_failure_degrades_instead_of_raising", test_store_failure_degrades_instead_of_raising);
    run("test_store_failure_on_first_request_still_degrades", test_store_failure_on_first_request_still_degrades);
    run("test_store_recovers", test_store_recovers);

    std::cout << "\nStage7ThreadSafety\n";
    run("test_same_tenant_never_oversells_under_contention", test_same_tenant_never_oversells_under_contention);
    run("test_tenants_keep_independent_quota_under_contention", test_tenants_keep_independent_quota_under_contention);
    run("test_hot_reload_races_with_traffic", test_hot_reload_races_with_traffic);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
