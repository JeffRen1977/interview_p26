#include "store.h"

void Store::set(const std::string& key,
                std::string value,
                std::optional<double> ttl,
                const std::vector<std::string>& tags) {
    // ttl is a duration from now, not an absolute timestamp.
    std::optional<double> expires_at =
        ttl.has_value() ? std::optional<double>(clock_.now() + *ttl) : std::nullopt;
    data_[key] = Entry{std::move(value), expires_at,
                       std::unordered_set<std::string>(tags.begin(), tags.end())};
}

std::optional<std::string> Store::get(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    // BUG: never consults TTL, so expired keys still return a value.
    return it->second.value;
}

bool Store::erase(const std::string& key) {
    return data_.erase(key) > 0;
}

bool Store::exists(const std::string& key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    return !is_expired(it->second);
}

int Store::size() {
    int n = 0;
    for (const auto& kv : data_) {
        if (!is_expired(kv.second)) {
            ++n;
        }
    }
    return n;
}

bool Store::is_expired(const Entry& entry) const {
    if (!entry.expires_at.has_value()) {
        return false;
    }
    return clock_.now() >= *entry.expires_at;
}

std::vector<std::string> Store::query(const std::vector<std::string>& /*tags*/,
                                      const std::string& /*match*/) {
    throw std::logic_error("Phase 2: implement query");
}

std::vector<std::string> Store::query_fast(const std::vector<std::string>& /*tags*/,
                                           const std::string& /*match*/) {
    throw std::logic_error("Phase 3: implement query_fast");
}
