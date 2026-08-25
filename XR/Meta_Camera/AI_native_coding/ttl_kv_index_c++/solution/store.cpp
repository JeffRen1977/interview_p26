#include "store.h"

#include <algorithm>
#include <stdexcept>

void Store::set(const std::string& key,
                std::string value,
                std::optional<double> ttl,
                const std::vector<std::string>& tags) {
    std::optional<double> expires_at =
        ttl.has_value() ? std::optional<double>(clock_.now() + *ttl) : std::nullopt;
    std::unordered_set<std::string> new_tags(tags.begin(), tags.end());
    auto it = data_.find(key);
    if (it != data_.end()) {
        index_remove(key, it->second.tags);
    }
    data_[key] = Entry{std::move(value), expires_at, std::move(new_tags)};
    index_add(key, data_[key].tags);
}

std::optional<std::string> Store::get(const std::string& key) {
    Entry* entry = touch(key);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry->value;
}

bool Store::erase(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    purge(key, it->second);
    return true;
}

bool Store::exists(const std::string& key) {
    return touch(key) != nullptr;
}

int Store::size() {
    int live = 0;
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    for (const auto& kv : data_) {
        keys.push_back(kv.first);
    }
    for (const auto& key : keys) {
        if (touch(key) != nullptr) {
            ++live;
        }
    }
    return live;
}

bool Store::is_expired(const Entry& entry) const {
    if (!entry.expires_at.has_value()) {
        return false;
    }
    return clock_.now() >= *entry.expires_at;
}

Entry* Store::touch(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return nullptr;
    }
    if (is_expired(it->second)) {
        purge(key, it->second);
        return nullptr;
    }
    return &it->second;
}

void Store::purge(const std::string& key, const Entry& entry) {
    index_remove(key, entry.tags);
    data_.erase(key);
}

void Store::index_add(const std::string& key, const std::unordered_set<std::string>& tags) {
    for (const auto& tag : tags) {
        by_tag_[tag].insert(key);
    }
}

void Store::index_remove(const std::string& key, const std::unordered_set<std::string>& tags) {
    for (const auto& tag : tags) {
        auto it = by_tag_.find(tag);
        if (it == by_tag_.end()) {
            continue;
        }
        it->second.erase(key);
        if (it->second.empty()) {
            by_tag_.erase(it);
        }
    }
}

bool Store::matches(const std::unordered_set<std::string>& have,
                    const std::vector<std::string>& needed,
                    const std::string& match) {
    if (match == "all") {
        for (const auto& tag : needed) {
            if (!have.count(tag)) {
                return false;
            }
        }
        return true;
    }
    if (match == "any") {
        for (const auto& tag : needed) {
            if (have.count(tag)) {
                return true;
            }
        }
        return false;
    }
    throw std::invalid_argument("unknown match mode: " + match);
}

std::vector<std::string> Store::query(const std::vector<std::string>& tags,
                                      const std::string& match) {
    std::vector<std::string> keys;
    keys.reserve(data_.size());
    for (const auto& kv : data_) {
        keys.push_back(kv.first);
    }

    std::vector<std::string> found;
    for (const auto& key : keys) {
        Entry* entry = touch(key);
        if (entry == nullptr) {
            continue;
        }
        if (matches(entry->tags, tags, match)) {
            found.push_back(key);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::vector<std::string> Store::query_fast(const std::vector<std::string>& tags,
                                           const std::string& match) {
    if (match == "all" && tags.empty()) {
        return query(tags, match);
    }
    if (match == "any" && tags.empty()) {
        return {};
    }

    std::unordered_set<std::string> candidates;
    if (match == "all") {
        bool first = true;
        for (const auto& tag : tags) {
            auto it = by_tag_.find(tag);
            if (it == by_tag_.end() || it->second.empty()) {
                return {};
            }
            if (first) {
                candidates = it->second;
                first = false;
            } else {
                std::unordered_set<std::string> next;
                for (const auto& key : candidates) {
                    if (it->second.count(key)) {
                        next.insert(key);
                    }
                }
                candidates.swap(next);
                if (candidates.empty()) {
                    return {};
                }
            }
        }
    } else if (match == "any") {
        for (const auto& tag : tags) {
            auto it = by_tag_.find(tag);
            if (it == by_tag_.end()) {
                continue;
            }
            candidates.insert(it->second.begin(), it->second.end());
        }
    } else {
        throw std::invalid_argument("unknown match mode: " + match);
    }

    std::vector<std::string> found;
    std::vector<std::string> keys(candidates.begin(), candidates.end());
    for (const auto& key : keys) {
        if (touch(key) != nullptr) {
            found.push_back(key);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}
