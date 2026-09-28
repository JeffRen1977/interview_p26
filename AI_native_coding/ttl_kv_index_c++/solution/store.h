// Reference solution — in-memory KV with TTL and a tag inverted index.
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

    void set(const std::string& key,
             std::string value,
             std::optional<double> ttl = std::nullopt,
             const std::vector<std::string>& tags = {});

    std::optional<std::string> get(const std::string& key);

    bool erase(const std::string& key);

    bool exists(const std::string& key);

    int size();

    std::vector<std::string> query(const std::vector<std::string>& tags,
                                   const std::string& match = "all");

    std::vector<std::string> query_fast(const std::vector<std::string>& tags,
                                        const std::string& match = "all");

private:
    bool is_expired(const Entry& entry) const;
    Entry* touch(const std::string& key);
    void purge(const std::string& key, const Entry& entry);
    void index_add(const std::string& key, const std::unordered_set<std::string>& tags);
    void index_remove(const std::string& key, const std::unordered_set<std::string>& tags);
    static bool matches(const std::unordered_set<std::string>& have,
                        const std::vector<std::string>& needed,
                        const std::string& match);

    Clock& clock_;
    std::unordered_map<std::string, Entry> data_;
    std::unordered_map<std::string, std::unordered_set<std::string>> by_tag_;
};
