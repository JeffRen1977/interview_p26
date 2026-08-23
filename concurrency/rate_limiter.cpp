// Problem: thread-safe rate limiters.
//
// Scenario: a camera uploads events to the cloud. Cap it at R events/sec with
// bursts up to B, shared across all uploader threads. Or: throttle how often
// a motion event triggers a push notification.
//
// Three implementations, increasing in sophistication:
//   1. Token bucket, mutex-based       -- the one to write first.
//   2. Token bucket, lock-free CAS     -- the follow-up if they ask.
//   3. Sliding window log              -- exact counts, more memory.
//
// The time question you must get right:
//   Use std::chrono::steady_clock, NEVER system_clock. system_clock can jump
//   backwards (NTP correction, user changing the clock) and your limiter will
//   either stall for hours or hand out infinite tokens. On a camera that has
//   just synced time after boot, this is a real bug, not a theoretical one.
//   Say this unprompted -- it reads as production experience.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "bench_scale.h"

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// 1. Token bucket, mutex-based.
// ---------------------------------------------------------------------------
// Tokens refill continuously at `rate` per second, capped at `burst`. Each
// request consumes one. Lazy refill: instead of a timer thread, compute how
// many tokens accrued since the last call. No background thread, no drift.
class TokenBucket {
 public:
    TokenBucket(double tokens_per_second, double burst)
        : rate_(tokens_per_second),
          burst_(burst),
          tokens_(burst),
          last_refill_(Clock::now()) {
        assert(rate_ > 0 && burst_ > 0);
    }

    bool try_acquire(double n = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        refill_locked();
        if (tokens_ >= n) {
            tokens_ -= n;
            return true;
        }
        ++rejected_;
        return false;
    }

    // Blocking variant: how long until n tokens are available, then sleep.
    // Compute the wait under the lock, sleep OUTSIDE it -- sleeping while
    // holding a mutex blocks every other thread. This is the mistake
    // interviewers watch for.
    void acquire(double n = 1.0) {
        for (;;) {
            std::chrono::nanoseconds wait{0};
            {
                std::lock_guard<std::mutex> lock(mutex_);
                refill_locked();
                if (tokens_ >= n) {
                    tokens_ -= n;
                    return;
                }
                const double deficit = n - tokens_;
                wait = std::chrono::nanoseconds(
                    static_cast<std::int64_t>(deficit / rate_ * 1e9));
            }
            std::this_thread::sleep_for(wait);
            // Loop rather than assuming success: another thread may have taken
            // the tokens while we slept. Same reason CV waits use a predicate.
        }
    }

    double available() {
        std::lock_guard<std::mutex> lock(mutex_);
        refill_locked();
        return tokens_;
    }

    std::uint64_t rejected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rejected_;
    }

 private:
    void refill_locked() {
        const auto now = Clock::now();
        const double elapsed_s =
            std::chrono::duration<double>(now - last_refill_).count();
        if (elapsed_s <= 0) {
            return;
        }
        tokens_ = std::min(burst_, tokens_ + elapsed_s * rate_);
        last_refill_ = now;
    }

    const double rate_;
    const double burst_;
    double tokens_;
    Clock::time_point last_refill_;
    std::uint64_t rejected_ = 0;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// 2. Lock-free token bucket (CAS loop).
// ---------------------------------------------------------------------------
// State is packed into ONE atomic so refill and consume are a single atomic
// transition -- that is the whole trick. Split across two atomics you would
// have the "atomic members don't make an atomic class" bug.
//
// We store tokens in fixed point (micro-tokens) and the timestamp in
// nanoseconds, in one 64-bit... which does not fit. So use a struct small
// enough to be lock-free on 64-bit: two 32-bit fields.
//
// Be honest in the interview: is_lock_free() may be false on some platforms,
// in which case the library takes a hidden lock and you have gained nothing.
// ALWAYS check, and always benchmark against the mutex version -- an
// uncontended mutex is ~20ns and very hard to beat.
struct BucketState {
    std::uint32_t micro_tokens;  // tokens * 1000
    std::uint32_t millis;        // ms since construction (wraps ~49 days)
};

class LockFreeTokenBucket {
 public:
    LockFreeTokenBucket(double tokens_per_second, double burst)
        : rate_milli_per_ms_(tokens_per_second),  // tokens/s == milli-tokens/ms
          burst_micro_(static_cast<std::uint32_t>(burst * 1000)),
          start_(Clock::now()) {
        state_.store(BucketState{burst_micro_, 0}, std::memory_order_relaxed);
    }

    static bool is_lock_free() {
        std::atomic<BucketState> probe;
        return probe.is_lock_free();
    }

    bool try_acquire() {
        const std::uint32_t now_ms = elapsed_ms();
        BucketState cur = state_.load(std::memory_order_acquire);
        for (;;) {
            // Recompute the whole next state from `cur` each iteration -- a
            // failed CAS means someone else moved it, so our arithmetic is
            // stale and must be redone. Getting this wrong is the classic
            // CAS-loop bug.
            const std::uint32_t dt =
                (now_ms > cur.millis) ? (now_ms - cur.millis) : 0;
            const double refilled =
                cur.micro_tokens + static_cast<double>(dt) * rate_milli_per_ms_;
            std::uint32_t tokens = static_cast<std::uint32_t>(
                std::min(refilled, static_cast<double>(burst_micro_)));

            if (tokens < 1000) {
                return false;  // less than one whole token
            }
            const BucketState next{tokens - 1000, now_ms};
            if (state_.compare_exchange_weak(cur, next,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
                return true;
            }
            // CAS failed: `cur` now holds the current value. Retry.
        }
    }

 private:
    std::uint32_t elapsed_ms() const {
        return static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                                  start_)
                .count());
    }

    const double rate_milli_per_ms_;
    const std::uint32_t burst_micro_;
    const Clock::time_point start_;
    std::atomic<BucketState> state_;
};

// ---------------------------------------------------------------------------
// 3. Sliding window log -- exact, at the cost of memory.
// ---------------------------------------------------------------------------
// Token bucket allows a burst at a window boundary (up to 2x the nominal rate
// across two adjacent windows). If the requirement is a hard "no more than N
// in ANY 60-second window", you need the log. Memory is O(N) per key, which is
// why you only use it for small N.
class SlidingWindowLimiter {
 public:
    SlidingWindowLimiter(std::size_t max_events,
                         std::chrono::milliseconds window)
        : max_events_(max_events), window_(window) {}

    bool try_acquire() {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        // Drop everything older than the window.
        while (!log_.empty() && now - log_.front() >= window_) {
            log_.pop_front();
        }
        if (log_.size() >= max_events_) {
            return false;
        }
        log_.push_back(now);
        return true;
    }

    std::size_t current_count() {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        while (!log_.empty() && now - log_.front() >= window_) {
            log_.pop_front();
        }
        return log_.size();
    }

 private:
    const std::size_t max_events_;
    const std::chrono::milliseconds window_;
    std::deque<Clock::time_point> log_;
    std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
void test_burst_then_throttle() {
    TokenBucket bucket(100.0, 10.0);  // 100/s, burst 10
    int granted = 0;
    for (int i = 0; i < 20; ++i) {
        if (bucket.try_acquire()) {
            ++granted;
        }
    }
    // Burst capacity is 10; a tiny amount may refill during the loop.
    assert(granted >= 10 && granted <= 12);
    assert(bucket.rejected() >= 8);
}

void test_refill_over_time() {
    TokenBucket bucket(1000.0, 5.0);
    while (bucket.try_acquire()) {
    }  // drain
    assert(bucket.available() < 1.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // ~20 tokens accrued, but the cap is 5.
    assert(bucket.available() >= 4.5);
    assert(bucket.available() <= 5.0);
}

// The property that actually matters: under concurrency, the TOTAL number of
// grants must never exceed burst + rate * elapsed. This catches a lost update
// in the refill path, which a single-threaded test never would.
void test_concurrent_never_exceeds_budget() {
    constexpr double kRate = 2000.0;
    constexpr double kBurst = 50.0;
    TokenBucket bucket(kRate, kBurst);

    std::atomic<int> granted{0};
    const auto start = Clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < scaled(5000, 200); ++i) {
                if (bucket.try_acquire()) {
                    granted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    const double elapsed_s =
        std::chrono::duration<double>(Clock::now() - start).count();
    const double budget = kBurst + kRate * elapsed_s;
    assert(granted.load() <= static_cast<int>(budget) + 1);
    std::cout << "  token bucket: granted=" << granted.load()
              << " budget=" << static_cast<int>(budget) << "\n";
}

void test_lock_free_bucket() {
    std::cout << "  lock-free bucket is_lock_free="
              << (LockFreeTokenBucket::is_lock_free() ? "yes" : "no") << "\n";
    constexpr double kRate = 2000.0;
    constexpr double kBurst = 50.0;
    LockFreeTokenBucket bucket(kRate, kBurst);

    std::atomic<int> granted{0};
    const auto start = Clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < scaled(5000, 200); ++i) {
                if (bucket.try_acquire()) {
                    granted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    const double elapsed_s =
        std::chrono::duration<double>(Clock::now() - start).count();
    const double budget = kBurst + kRate * elapsed_s;
    // Millisecond time resolution makes this slightly lumpy; allow slack.
    assert(granted.load() <= static_cast<int>(budget) + kRate * 0.01 + 10);
    std::cout << "  lock-free   : granted=" << granted.load()
              << " budget=" << static_cast<int>(budget) << "\n";
}

void test_sliding_window() {
    SlidingWindowLimiter limiter(5, std::chrono::milliseconds(50));
    int granted = 0;
    for (int i = 0; i < 10; ++i) {
        if (limiter.try_acquire()) {
            ++granted;
        }
    }
    assert(granted == 5);              // exact, no burst overshoot
    assert(!limiter.try_acquire());
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(limiter.current_count() == 0);
    assert(limiter.try_acquire());
}

void test_blocking_acquire() {
    TokenBucket bucket(500.0, 2.0);
    bucket.acquire();
    bucket.acquire();
    const auto start = Clock::now();
    bucket.acquire();  // must wait ~2ms for a token
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    assert(ms >= 1.0);
    std::cout << "  blocking acquire waited " << ms << " ms\n";
}

int main() {
    test_burst_then_throttle();
    test_refill_over_time();
    test_concurrent_never_exceeds_budget();
    test_lock_free_bucket();
    test_sliding_window();
    test_blocking_acquire();
    std::cout << "rate_limiter: ok\n";
    return 0;
}
