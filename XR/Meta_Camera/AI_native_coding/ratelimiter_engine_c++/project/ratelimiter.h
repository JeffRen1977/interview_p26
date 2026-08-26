// Sliding-window, cost-weighted, multi-tenant rate limiter.
//
// Stage 1 default window        Stage 5 hot-reload rules
// Stage 2 tenant + endpoint     Stage 6 soft degrade + store failure
// Stage 3 weighted cost         Stage 7 thread safety
// Stage 4 sliding window (injected timestamps)
#pragma once

#include "memory_store.h"
#include "request.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

// Gateway action for one request.
//   Allow   -- forward to origin
//   Degrade -- do not hit origin; serve fallback / cached / cheaper path
//   Deny    -- reject (HTTP 429)
enum class Decision { Allow, Degrade, Deny };

const char* to_string(Decision decision);

struct Rule {
    int limit = 0;
    long long window_ms = 0;
    int degrade_threshold = 0;
};

// One mutex per quota key so tenant A does not block tenant B.
class KeyedLocks {
public:
    // Returns a reference to this key's mutex, creating it on first use.
    //
    // std::mutex is neither copyable nor movable, so the map must construct it
    // in place (try_emplace). unordered_map is node-based, so the reference
    // stays valid across a rehash -- which is exactly what makes it safe to
    // hold this reference while other threads insert new keys.
    std::mutex& for_key(const std::string& key);

private:
    std::mutex guard_;
    std::unordered_map<std::string, std::mutex> locks_;
};

// Authoritative state lives in `store`, never in the limiter's own fields.
//
// Key:   "{user_id}:{endpoint}" so tenants and routes are isolated.
// Value: the events still relevant to the current window.
class RateLimiter {
public:
    // Throws std::invalid_argument on a negative limit or a non-positive window.
    explicit RateLimiter(MemoryStore& store,
                         int default_limit = 10,
                         long long default_window_ms = 1000,
                         std::optional<int> default_degrade_threshold = std::nullopt);

    // Hot-reload per-endpoint quota. The next allow() uses the new limit/window.
    // degrade_threshold defaults to `limit` (no soft band) and must be in
    // [0, limit].
    void set_rule(const std::string& endpoint,
                  int limit,
                  long long window_ms,
                  std::optional<int> degrade_threshold = std::nullopt);

    // Decide one request.
    //   Allow   -- used + cost <= degrade_threshold
    //   Degrade -- over the soft threshold but still <= limit, OR the store threw
    //   Deny    -- used + cost > limit  (and the request consumes nothing)
    Decision allow(const Request& request);

private:
    Rule rule_for(const std::string& endpoint) const;
    Decision allow_locked(const std::string& key, const Rule& rule, const Request& request);

    MemoryStore& store_;
    Rule default_rule_;
    mutable std::mutex rules_mutex_;
    std::unordered_map<std::string, Rule> rules_;
    KeyedLocks key_locks_;
};
