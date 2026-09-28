// In-memory KV store with TTL and tag queries.
//
// Phase 1 lives in get / set / exists: two of these helpers do not honour the
// contract in the comments. The tests tell you which.
//
// Phase 2: query — scan is fine.
// Phase 3: query_fast — the suite hammers a rare tag in a tight loop; a scan dies.
//
// C++ note: Python's delete() is erase() here (delete is a keyword).

#pragma once

#include "clock.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Entry {
    std::string value;
    std::optional<double> expires_at;
    std::unordered_set<std::string> tags;
};

class Store {
public:
    explicit Store(Clock& clock) : clock_(clock) {}

    // Insert or replace `key`.
    // `ttl` is seconds from *now* (this clock). nullopt means never expire.
    // `tags` is used by query(); missing tags means no tags.
    // Overwriting a key replaces value, ttl, and tags.
    void set(const std::string& key,
             std::string value,
             std::optional<double> ttl = std::nullopt,
             const std::vector<std::string>& tags = {});

    // Return the value, or nullopt if missing or expired.
    // Expired keys must be treated as a miss (lazy eviction is allowed).
    // A key expires at the instant clock.now() >= expires_at.
    std::optional<std::string> get(const std::string& key);

    // Remove `key`. True iff it was present (even if already expired).
    bool erase(const std::string& key);

    // True iff the key is present and not expired.
    bool exists(const std::string& key);

    // Number of keys that are present and not expired.
    int size();

    // Return live keys whose tags match, sorted.
    // match="all": key must contain every requested tag (empty tags → every live key).
    // match="any": key must contain at least one requested tag (empty tags → []).
    // Skip expired keys. Scanning is fine.
    std::vector<std::string> query(const std::vector<std::string>& tags,
                                   const std::string& match = "all");

    // Same answers as query, but it must survive the tight-loop stress set.
    std::vector<std::string> query_fast(const std::vector<std::string>& tags,
                                        const std::string& match = "all");

private:
    bool is_expired(const Entry& entry) const;

    Clock& clock_;
    std::unordered_map<std::string, Entry> data_;
};
