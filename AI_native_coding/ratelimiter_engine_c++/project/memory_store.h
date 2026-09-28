// Process-local KV stand-in for Redis / Memcached.
//
// The limiter must not keep authoritative quota in its own fields. Swap this
// class for a Redis client later; the engine API stays the same.
//
// Note vs the Python version: there the store is a generic `dict[str, Any]`.
// Here it is typed to the one value shape the limiter needs. C++ could use
// std::any, but a typed boundary catches the mistake at compile time -- and a
// real Redis client would serialise anyway, so "generic" buys nothing.
#pragma once

#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// (timestamp_ms, cost)
using Event = std::pair<long long, int>;
using EventList = std::vector<Event>;

// Thrown when the backing store cannot complete a read or write.
struct StoreError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class MemoryStore {
public:
    // get/set/erase are mutex-protected so concurrent tenants can share one
    // store. Values are copied so callers never alias the stored vector.
    std::optional<EventList> get(const std::string& key);
    void set(const std::string& key, EventList value);
    void erase(const std::string& key);

    // Test hook: flip to make every operation raise StoreError.
    void set_failing(bool failing);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, EventList> kv_;
    bool fail_ = false;
};
