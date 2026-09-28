#include "ratelimiter.h"

#include <stdexcept>
#include <utility>

const char* to_string(Decision decision) {
    switch (decision) {
        case Decision::Allow: return "allow";
        case Decision::Degrade: return "degrade";
        case Decision::Deny: return "deny";
    }
    return "?";
}

std::mutex& KeyedLocks::for_key(const std::string& key) {
    std::lock_guard<std::mutex> guard(guard_);
    return locks_.try_emplace(key).first->second;
}

RateLimiter::RateLimiter(MemoryStore& store,
                         int default_limit,
                         long long default_window_ms,
                         std::optional<int> default_degrade_threshold)
    : store_(store) {
    if (default_limit < 0 || default_window_ms <= 0) {
        throw std::invalid_argument("default_limit must be >= 0 and default_window_ms > 0");
    }
    const int degrade = default_degrade_threshold.value_or(default_limit);
    default_rule_ = Rule{default_limit, default_window_ms, degrade};
}

void RateLimiter::set_rule(const std::string& endpoint,
                           int limit,
                           long long window_ms,
                           std::optional<int> degrade_threshold) {
    if (limit < 0 || window_ms <= 0) {
        throw std::invalid_argument("limit must be >= 0 and window_ms > 0");
    }
    const int degrade = degrade_threshold.value_or(limit);
    if (degrade < 0 || degrade > limit) {
        throw std::invalid_argument("degrade_threshold must be in [0, limit]");
    }
    std::lock_guard<std::mutex> guard(rules_mutex_);
    rules_[endpoint] = Rule{limit, window_ms, degrade};
}

Rule RateLimiter::rule_for(const std::string& endpoint) const {
    std::lock_guard<std::mutex> guard(rules_mutex_);
    const auto it = rules_.find(endpoint);
    return it == rules_.end() ? default_rule_ : it->second;
}

Decision RateLimiter::allow(const Request& request) {
    if (request.cost < 0) throw std::invalid_argument("cost must be >= 0");

    // Snapshot the rule ONCE, outside the per-key lock. Re-reading it mid-decision
    // would let a concurrent set_rule split one decision across two rules.
    const Rule rule = rule_for(request.endpoint);
    const std::string key = request.user_id + ":" + request.endpoint;

    std::lock_guard<std::mutex> guard(key_locks_.for_key(key));
    return allow_locked(key, rule, request);
}

// The read-modify-write of one quota key runs under that key's mutex, so two
// workers for the same tenant cannot oversell while other tenants proceed.
Decision RateLimiter::allow_locked(const std::string& key,
                                   const Rule& rule,
                                   const Request& request) {
    EventList events;
    try {
        if (auto stored = store_.get(key)) events = std::move(*stored);
    } catch (const StoreError&) {
        return Decision::Degrade;  // fail open, but degraded: keep the gateway up
    }

    // Sliding window: an event at t expires once now - window_ms reaches it,
    // i.e. keep strictly newer than the cutoff.
    const long long cutoff = request.timestamp - rule.window_ms;
    EventList live;
    live.reserve(events.size());
    long long used = 0;
    for (const auto& [ts, cost] : events) {
        if (ts > cutoff) {
            live.emplace_back(ts, cost);
            used += cost;
        }
    }

    const long long projected = used + request.cost;

    if (projected > rule.limit) {
        // A denied request consumes nothing, but we still persist the pruned
        // window so the expiry work is not repeated on every rejected call.
        try {
            store_.set(key, live);
        } catch (const StoreError&) {
            return Decision::Degrade;
        }
        return Decision::Deny;
    }

    live.emplace_back(request.timestamp, request.cost);
    try {
        store_.set(key, std::move(live));
    } catch (const StoreError&) {
        return Decision::Degrade;
    }

    return projected > rule.degrade_threshold ? Decision::Degrade : Decision::Allow;
}
