#include "memory_store.h"

std::optional<EventList> MemoryStore::get(const std::string& key) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (fail_) throw StoreError("store unavailable");
    const auto it = kv_.find(key);
    if (it == kv_.end()) return std::nullopt;
    return it->second;  // copy on purpose
}

void MemoryStore::set(const std::string& key, EventList value) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (fail_) throw StoreError("store unavailable");
    kv_[key] = std::move(value);
}

void MemoryStore::erase(const std::string& key) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (fail_) throw StoreError("store unavailable");
    kv_.erase(key);
}

void MemoryStore::set_failing(bool failing) {
    std::lock_guard<std::mutex> guard(mutex_);
    fail_ = failing;
}
