// Interview whiteboard: lazy token-bucket rate limiter (EC2 datapath style).
//
// Key ideas:
// 1. No background timer thread. Refill lazily on packet arrival from the
//    hardware-timestamp delta. Timer threads can't hit microsecond precision at
//    millions of pps and add context-switch overhead.
// 2. Token bucket = rate (steady) + capacity (burst).
// 3. Use double tokens so sub-token-per-microsecond refills accumulate instead
//    of truncating to zero.
//
// Follow-ups (see docs/11): multi-core → RSS pin to one core (lock-free), or
// per-core local buckets with amortized bulk refill from a global bucket.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>

class EC2PacketRateLimiter {
 public:
    // rate_per_sec: sustained packets/sec. burst_size: max bucket capacity.
    EC2PacketRateLimiter(std::uint64_t rate_per_sec, std::uint64_t burst_size)
        : capacity_(static_cast<double>(burst_size)),
          tokens_(static_cast<double>(burst_size)),
          rate_per_us_(static_cast<double>(rate_per_sec) / 1'000'000.0),
          last_update_us_(0) {}

    // Returns true to admit the packet, false to drop (rate exceeded).
    // now_us is injected so the hot path can feed RDTSC / NIC timestamps and so
    // the logic is deterministically testable.
    bool AllowPacket(std::uint64_t now_us) {
        // Lazy refill: add tokens for elapsed time since the last packet.
        // Guard against non-monotonic timestamps (clock jump / reordering):
        // never let elapsed go negative, but always advance last_update_us_.
        if (now_us > last_update_us_) {
            const double elapsed_us =
                static_cast<double>(now_us - last_update_us_);
            tokens_ = std::min(capacity_, tokens_ + elapsed_us * rate_per_us_);
        }
        last_update_us_ = now_us;

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

    double tokens() const { return tokens_; }

 private:
    double capacity_;
    double tokens_;
    double rate_per_us_;
    std::uint64_t last_update_us_;
};

// ---------------------------------------------------------------------------
// Tests use injected timestamps for determinism (no wall-clock flakiness).
// ---------------------------------------------------------------------------

void test_burst_then_throttle() {
    // 1000 pps, burst 10. At t=0 we can send the whole burst, then must wait.
    EC2PacketRateLimiter limiter(1000, 10);

    int admitted = 0;
    for (int i = 0; i < 20; ++i) {
        if (limiter.AllowPacket(0)) {
            ++admitted;
        }
    }
    assert(admitted == 10);  // only the burst capacity passes at t=0
}

void test_lazy_refill() {
    // 1000 pps → 1 token per 1000 us. Burst 1 so refill timing is exact.
    EC2PacketRateLimiter limiter(1000, 1);

    assert(limiter.AllowPacket(0));       // spend the initial token
    assert(!limiter.AllowPacket(0));      // empty immediately after
    assert(!limiter.AllowPacket(500));    // 0.5 token after 500us: still < 1
    assert(limiter.AllowPacket(1000));    // 1 full token accrued at 1000us
}

void test_sub_token_precision() {
    // The double-vs-int trap: 1000 pps = 0.001 token/us. Packets arriving a few
    // microseconds apart must still accumulate fractional tokens over time.
    EC2PacketRateLimiter limiter(1000, 1);
    assert(limiter.AllowPacket(0));

    // 200 arrivals spaced 5us apart = 1000us total → exactly one token back.
    int admitted = 0;
    for (int i = 1; i <= 200; ++i) {
        if (limiter.AllowPacket(static_cast<std::uint64_t>(i) * 5)) {
            ++admitted;
        }
    }
    // With integer math each 5us step would floor to 0 tokens and admit none.
    assert(admitted == 1);
}

void test_capacity_cap() {
    // Long idle period must not overflow the bucket beyond capacity.
    EC2PacketRateLimiter limiter(1000, 10);
    // Idle 10 seconds; refill is capped at capacity (10), not 10000.
    limiter.AllowPacket(10'000'000);  // triggers refill + spends one
    int admitted = 1;
    for (int i = 0; i < 100; ++i) {
        if (limiter.AllowPacket(10'000'000)) {
            ++admitted;
        }
    }
    assert(admitted == 10);
}

void test_clock_regression_is_safe() {
    EC2PacketRateLimiter limiter(1000, 5);
    assert(limiter.AllowPacket(1000));
    // A backward timestamp must not add tokens or crash.
    limiter.AllowPacket(500);
    assert(limiter.tokens() <= 5.0);
}

int main() {
    test_burst_then_throttle();
    test_lazy_refill();
    test_sub_token_precision();
    test_capacity_cap();
    test_clock_regression_is_safe();
    std::cout << "13_token_bucket_rate_limiter: ok\n";
    return 0;
}
