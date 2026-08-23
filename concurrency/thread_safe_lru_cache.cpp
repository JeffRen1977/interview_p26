// Problem: thread-safe LRU cache with a TTL.
//
// Scenario: a gateway caches recently-fetched clip metadata / thumbnails so
// repeated UI queries don't hit the archive. Bounded size, evict least
// recently used, entries expire.
//
// This is the classic "LeetCode 146 + make it thread-safe" question, and the
// interesting part is entirely in the second half. Points to make:
//
// - The single-thread design is unchanged: hash map -> list iterator, plus an
//   intrusive list ordered MRU..LRU. O(1) get and put.
//
// - Naive thread-safety is ONE mutex over both structures. Note the trap:
//   `get` MUTATES the list (it moves the node to the front), so it is NOT a
//   read operation. You cannot use a shared_lock for get() in a strict LRU.
//   Interviewers love asking "can't get() take a read lock?" -- the answer is
//   no, and knowing why is the point.
//
// - If you want concurrent reads, you must relax the eviction policy:
//     * CLOCK / second-chance: get() only sets an atomic `referenced` bit,
//       which is a pure write to a per-entry atomic, no list surgery. That
//       makes get() safe under a shared_lock. Approximates LRU well.
//     * Sharding: N independent caches keyed by hash(key) % N. Cuts contention
//       by N with no policy change. Global LRU order is lost (each shard has
//       its own), which is almost always fine.
//   Both are implemented below.
//
// - Say this: "the standard answer to lock contention on a map is sharding,
//   and the standard price is losing any global ordering property."

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bench_scale.h"

// ---------------------------------------------------------------------------
// 1. Strict LRU under one mutex.
// ---------------------------------------------------------------------------
template <typename K, typename V>
class LruCache {
 public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {
        assert(capacity_ > 0);
    }

    // NOT const, and NOT shared-lockable: it reorders the list.
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) {
            ++misses_;
            return std::nullopt;
        }
        // splice moves the node without invalidating the iterator we stored --
        // that stability is exactly why std::list is the right container here.
        order_.splice(order_.begin(), order_, it->second);
        ++hits_;
        return it->second->second;
    }

    void put(const K& key, V value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = std::move(value);
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        if (index_.size() >= capacity_) {
            const K& victim = order_.back().first;
            index_.erase(victim);
            order_.pop_back();
            ++evictions_;
        }
        order_.emplace_front(key, std::move(value));
        index_[key] = order_.begin();
    }

    bool erase(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) {
            return false;
        }
        order_.erase(it->second);
        index_.erase(it);
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }
    std::uint64_t hits() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return hits_;
    }
    std::uint64_t evictions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return evictions_;
    }

 private:
    using Entry = std::pair<K, V>;
    using ListIt = typename std::list<Entry>::iterator;

    const std::size_t capacity_;
    std::list<Entry> order_;  // front = most recently used
    std::unordered_map<K, ListIt> index_;
    std::uint64_t hits_ = 0, misses_ = 0, evictions_ = 0;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// 2. Sharded LRU -- the standard scalability answer.
// ---------------------------------------------------------------------------
// N independent LruCache instances. Two threads touching different keys almost
// never contend. Capacity is split across shards, so a skewed key distribution
// can evict earlier than a global LRU would -- state that trade-off.
template <typename K, typename V, std::size_t kShards = 16>
class ShardedLruCache {
 public:
    explicit ShardedLruCache(std::size_t total_capacity) {
        const std::size_t per_shard =
            std::max<std::size_t>(1, total_capacity / kShards);
        shards_.reserve(kShards);
        for (std::size_t i = 0; i < kShards; ++i) {
            shards_.push_back(std::make_unique<LruCache<K, V>>(per_shard));
        }
    }

    std::optional<V> get(const K& key) { return shard_for(key).get(key); }
    void put(const K& key, V value) { shard_for(key).put(key, std::move(value)); }
    bool erase(const K& key) { return shard_for(key).erase(key); }

    std::size_t size() const {
        std::size_t n = 0;
        for (const auto& s : shards_) {
            n += s->size();
        }
        return n;
    }

 private:
    LruCache<K, V>& shard_for(const K& key) const {
        // Mix the hash: some std::hash implementations are the identity for
        // integers, which would send sequential keys to the same shard after
        // the modulo.
        std::size_t h = std::hash<K>{}(key);
        h ^= (h >> 32);
        h *= 0x9e3779b97f4a7c15ULL;
        h ^= (h >> 29);
        return *shards_[h % kShards];
    }

    std::vector<std::unique_ptr<LruCache<K, V>>> shards_;
};

// ---------------------------------------------------------------------------
// 3. CLOCK cache -- concurrent reads, at the cost of exact LRU.
// ---------------------------------------------------------------------------
// get() takes only a SHARED lock: the hit path does not restructure anything,
// it just sets a per-entry atomic bit. Eviction sweeps a hand around the ring,
// clearing set bits and evicting the first entry it finds already clear
// (second chance). This is what real caches (and OS page replacement) do.
template <typename K, typename V>
class ClockCache {
 public:
    explicit ClockCache(std::size_t capacity)
        : capacity_(capacity), slots_(capacity) {
        assert(capacity_ > 0);
    }

    // Concurrent with other get()s. Only put()/evict take the exclusive lock.
    std::optional<V> get(const K& key) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) {
            return std::nullopt;
        }
        Slot& s = slots_[it->second];
        // Pure atomic store -- no list surgery, so shared_lock is sound.
        s.referenced.store(true, std::memory_order_relaxed);
        return s.value;
    }

    void put(const K& key, V value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it != index_.end()) {
            slots_[it->second].value = std::move(value);
            slots_[it->second].referenced.store(true, std::memory_order_relaxed);
            return;
        }
        std::size_t slot = (index_.size() < capacity_) ? index_.size()
                                                       : evict_one_locked();
        slots_[slot].key = key;
        slots_[slot].value = std::move(value);
        slots_[slot].occupied = true;
        slots_[slot].referenced.store(true, std::memory_order_relaxed);
        index_[key] = slot;
    }

    std::size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return index_.size();
    }

 private:
    struct Slot {
        K key{};
        V value{};
        bool occupied = false;
        std::atomic<bool> referenced{false};
    };

    // Precondition: exclusive lock held.
    std::size_t evict_one_locked() {
        for (;;) {
            Slot& s = slots_[hand_];
            const std::size_t current = hand_;
            hand_ = (hand_ + 1) % capacity_;
            if (!s.occupied) {
                return current;
            }
            if (s.referenced.exchange(false, std::memory_order_relaxed)) {
                continue;  // second chance: it was used, clear and move on
            }
            index_.erase(s.key);
            s.occupied = false;
            return current;
        }
    }

    const std::size_t capacity_;
    std::vector<Slot> slots_;
    std::unordered_map<K, std::size_t> index_;
    std::size_t hand_ = 0;
    mutable std::shared_mutex mutex_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
void test_lru_semantics() {
    LruCache<int, std::string> cache(2);
    cache.put(1, "one");
    cache.put(2, "two");
    assert(cache.get(1).value() == "one");  // 1 becomes MRU
    cache.put(3, "three");                  // evicts 2, not 1
    assert(!cache.get(2).has_value());
    assert(cache.get(1).value() == "one");
    assert(cache.get(3).value() == "three");
    assert(cache.size() == 2);
    assert(cache.evictions() == 1);

    assert(cache.erase(1));
    assert(!cache.erase(1));
    assert(cache.size() == 1);
}

void test_lru_update_existing() {
    LruCache<int, int> cache(2);
    cache.put(1, 100);
    cache.put(1, 200);           // update, not insert
    assert(cache.size() == 1);
    assert(cache.get(1).value() == 200);
}

void test_concurrent_lru() {
    LruCache<int, int> cache(256);
    constexpr int kThreads = 8;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < scaled(20000, 500); ++i) {
                const int key = (t * 7 + i) % 1000;
                if (auto v = cache.get(key)) {
                    assert(*v == key * 2);  // value integrity under churn
                } else {
                    cache.put(key, key * 2);
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(cache.size() <= 256);  // capacity invariant never violated
}

void test_sharded() {
    ShardedLruCache<int, int, 8> cache(800);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < scaled(20000, 500); ++i) {
                const int key = (t * 13 + i) % 2000;
                if (auto v = cache.get(key)) {
                    assert(*v == key * 3);
                } else {
                    cache.put(key, key * 3);
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(cache.size() <= 800);
}

void test_clock() {
    ClockCache<int, int> cache(64);
    for (int i = 0; i < 64; ++i) {
        cache.put(i, i);
    }
    assert(cache.size() == 64);
    // Keep 0..31 hot, then insert more; the hot half should mostly survive.
    for (int round = 0; round < 3; ++round) {
        for (int i = 0; i < 32; ++i) {
            cache.get(i);
        }
        for (int i = 100 + round * 8; i < 108 + round * 8; ++i) {
            cache.put(i, i);
        }
    }
    int survivors = 0;
    for (int i = 0; i < 32; ++i) {
        if (cache.get(i)) {
            ++survivors;
        }
    }
    assert(survivors >= 24);  // second chance protected the hot set
    assert(cache.size() <= 64);

    // Concurrent readers under shared_lock.
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < scaled(20000, 500); ++i) {
                cache.get(i % 64);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    test_lru_semantics();
    test_lru_update_existing();
    test_concurrent_lru();
    test_sharded();
    test_clock();
    std::cout << "thread_safe_lru_cache: ok\n";
    return 0;
}
