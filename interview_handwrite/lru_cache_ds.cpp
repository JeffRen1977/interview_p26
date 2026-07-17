// LRU cache — std::list + unordered_map.
//
// For the raw-pointer / custom Node version preferred in systems interviews,
// see lru_cache_raw_list.cpp.
//
// Whiteboard talking points:
// - unordered_map gives O(1) lookup to a list iterator.
// - List order is MRU (front) -> LRU (back).
// - splice() moves a node in O(1) without copying the payload.
// - ThreadSafeLRUCache wraps the core cache with one coarse mutex.

#include <cassert>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

template <typename K, typename V>
class LRUCache {
 public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be positive");
        }
    }

    std::optional<V> get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }
        touch(it->second);
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            touch(it->second);
            return;
        }

        items_.emplace_front(key, value);
        map_[key] = items_.begin();

        if (map_.size() > capacity_) {
            auto lru = std::prev(items_.end());
            map_.erase(lru->first);
            items_.erase(lru);
        }
    }

    size_t size() const { return map_.size(); }

    std::list<K> keys_from_mru_to_lru() const {
        std::list<K> keys;
        for (const auto& [key, value] : items_) {
            (void)value;
            keys.push_back(key);
        }
        return keys;
    }

 private:
    using Entry = std::pair<K, V>;
    using EntryList = std::list<Entry>;
    using EntryIt = typename EntryList::iterator;

    size_t capacity_;
    EntryList items_;
    std::unordered_map<K, EntryIt> map_;

    void touch(EntryIt it) {
        items_.splice(items_.begin(), items_, it);
    }
};

template <typename K, typename V>
class ThreadSafeLRUCache {
 public:
    explicit ThreadSafeLRUCache(size_t capacity) : cache_(capacity) {}

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.get(key);
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.put(key, value);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

 private:
    LRUCache<K, V> cache_;
    mutable std::mutex mutex_;
};

bool test_lru_cache_ds() {
    LRUCache<int, std::string> cache(2);
    cache.put(1, "a");
    cache.put(2, "b");
    assert(cache.get(1) == "a");
    cache.put(3, "c");
    assert(!cache.get(2).has_value());
    assert(cache.get(3) == "c");

    auto keys = cache.keys_from_mru_to_lru();
    assert(keys.size() == 2);
    assert(keys.front() == 3);
    assert(keys.back() == 1);

    ThreadSafeLRUCache<int, int> safe(2);
    safe.put(1, 10);
    assert(safe.get(1) == 10);
    return true;
}

int main() {
    assert(test_lru_cache_ds());
    std::cout << "lru_cache_ds: ok\n";
    return 0;
}
