// Problem: read-mostly shared state -- four strategies, one benchmark.
//
// Runnable companion to 读多写少.md. Scenario: a camera-configuration /
// device-status map. Thousands of reads per second (every frame checks
// "is analytics enabled for this camera?"), a write every few minutes.
//
// The four strategies, in the order you should mention them:
//
//   1. std::mutex           -- baseline. Correct, simple, serializes readers.
//   2. std::shared_mutex    -- readers share, writer excludes. Wins when the
//                              read critical section is LONG. For a one-line
//                              hash lookup, the shared_mutex bookkeeping can
//                              cost more than the mutex it replaces -- say
//                              this, it shows you have measured rather than
//                              cargo-culted.
//   3. Copy-on-write        -- readers are lock-free (one atomic load + a
//                              refcount bump). Writers copy the whole map.
//                              Great for small, rarely-written data.
//                              Cost: O(n) per write, and readers can hold an
//                              old snapshot alive.
//   4. RCU / double-buffer  -- what you'd name-drop for the kernel-style
//                              answer; COW is the userspace-friendly cousin.
//
// The decision rule to state:
//   write ratio high  -> plain mutex
//   long reads        -> shared_mutex
//   tiny data, ~never written, reads on the hot path -> COW
//
// Correctness trap to volunteer (interviewers love this one):
//   shared_lock does NOT make the objects you read thread-safe -- it only
//   guarantees no writer runs concurrently. If a reader hands out a reference
//   or iterator that outlives the lock, you have a dangling read. Return by
//   value, or return a shared_ptr snapshot.

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bench_scale.h"

struct CameraConfig {
    bool analytics_enabled = false;
    int bitrate_kbps = 0;
    int retention_days = 0;
};

using ConfigMap = std::unordered_map<std::uint32_t, CameraConfig>;

// ---------------------------------------------------------------------------
// 1. Plain mutex -- the baseline you should always write first.
// ---------------------------------------------------------------------------
class MutexConfigStore {
 public:
    CameraConfig get(std::uint32_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(id);
        return it == map_.end() ? CameraConfig{} : it->second;  // by VALUE
    }
    void set(std::uint32_t id, const CameraConfig& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_[id] = cfg;
    }

 private:
    ConfigMap map_;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// 2. shared_mutex -- concurrent readers, exclusive writer.
// ---------------------------------------------------------------------------
class SharedMutexConfigStore {
 public:
    CameraConfig get(std::uint32_t id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);  // shared
        auto it = map_.find(id);
        return it == map_.end() ? CameraConfig{} : it->second;
    }

    // Read many keys under ONE lock. This is where shared_mutex actually pays:
    // a long critical section that would otherwise serialize every reader.
    std::vector<CameraConfig> get_many(
        const std::vector<std::uint32_t>& ids) const {
        std::vector<CameraConfig> out;
        out.reserve(ids.size());
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (std::uint32_t id : ids) {
            auto it = map_.find(id);
            out.push_back(it == map_.end() ? CameraConfig{} : it->second);
        }
        return out;
    }

    void set(std::uint32_t id, const CameraConfig& cfg) {
        std::unique_lock<std::shared_mutex> lock(mutex_);  // exclusive
        map_[id] = cfg;
    }

 private:
    ConfigMap map_;
    mutable std::shared_mutex mutex_;
};

// ---------------------------------------------------------------------------
// 3. Copy-on-write -- lock-free reads.
// ---------------------------------------------------------------------------
// Note on portability: std::atomic<std::shared_ptr<T>> is C++20. In C++17 you
// use the free functions std::atomic_load/atomic_store on a shared_ptr, which
// are deprecated in C++20. This file uses the free-function form so it builds
// as C++17; mention both in the interview.
class CowConfigStore {
 public:
    CowConfigStore() { std::atomic_store(&map_, std::make_shared<const ConfigMap>()); }

    // NO LOCK on the read path. One atomic load + a refcount increment.
    CameraConfig get(std::uint32_t id) const {
        // Take a snapshot. The returned shared_ptr keeps that version alive
        // even if a writer swaps in a new one a nanosecond later -- which is
        // exactly what makes this safe.
        std::shared_ptr<const ConfigMap> snap = std::atomic_load(&map_);
        auto it = snap->find(id);
        return it == snap->end() ? CameraConfig{} : it->second;
    }

    // Writers serialize among themselves and pay O(n) to copy.
    void set(std::uint32_t id, const CameraConfig& cfg) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        auto old_map = std::atomic_load(&map_);
        auto new_map = std::make_shared<ConfigMap>(*old_map);  // copy
        (*new_map)[id] = cfg;                                  // modify
        std::atomic_store(&map_,
                          std::shared_ptr<const ConfigMap>(new_map));  // swap
    }

    // A reader that needs a consistent view of MANY keys gets it for free:
    // one snapshot, no lock, no torn read across keys.
    std::shared_ptr<const ConfigMap> snapshot() const {
        return std::atomic_load(&map_);
    }

 private:
    std::shared_ptr<const ConfigMap> map_;
    std::mutex write_mutex_;  // serializes writers only; readers never take it
};

// ---------------------------------------------------------------------------
// Benchmark: many readers, one slow writer.
// ---------------------------------------------------------------------------
template <typename Store>
double bench(const char* name, int reader_threads, int reads_per_thread,
             int keys) {
    Store store;
    for (int i = 0; i < keys; ++i) {
        store.set(static_cast<std::uint32_t>(i),
                  CameraConfig{true, 2000 + i, 30});
    }

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> checksum{0};

    // One writer, updating slowly -- the read-mostly assumption.
    std::thread writer([&] {
        int n = 0;
        while (!stop.load(std::memory_order_acquire)) {
            store.set(static_cast<std::uint32_t>(n % keys),
                      CameraConfig{true, 3000 + n, 30});
            ++n;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> readers;
    for (int t = 0; t < reader_threads; ++t) {
        readers.emplace_back([&, t] {
            std::uint64_t local = 0;
            for (int i = 0; i < reads_per_thread; ++i) {
                local += static_cast<std::uint64_t>(
                    store.get(static_cast<std::uint32_t>((i + t) % keys))
                        .bitrate_kbps);
            }
            checksum.fetch_add(local, std::memory_order_relaxed);
        });
    }
    for (auto& r : readers) {
        r.join();
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    stop.store(true, std::memory_order_release);
    writer.join();

    const double ms =
        std::chrono::duration<double, std::milli>(elapsed).count();
    std::cout << "  " << name << ": " << ms << " ms for "
              << (reader_threads * reads_per_thread) << " reads"
              << " (checksum " << checksum.load() << ")\n";
    return ms;
}

// ---------------------------------------------------------------------------
// Correctness tests
// ---------------------------------------------------------------------------
template <typename Store>
void test_basic() {
    Store store;
    store.set(1, CameraConfig{true, 4000, 30});
    assert(store.get(1).bitrate_kbps == 4000);
    assert(store.get(999).bitrate_kbps == 0);  // missing -> default
    store.set(1, CameraConfig{false, 8000, 60});
    assert(store.get(1).bitrate_kbps == 8000);
    assert(store.get(1).analytics_enabled == false);
}

// A COW snapshot must stay stable even while writers churn -- this is the
// property that makes COW nice for "consistent multi-key read".
void test_cow_snapshot_is_stable() {
    CowConfigStore store;
    for (int i = 0; i < 16; ++i) {
        store.set(static_cast<std::uint32_t>(i), CameraConfig{true, i, 30});
    }
    auto snap = store.snapshot();
    const std::size_t before = snap->size();

    std::thread writer([&] {
        for (int i = 100; i < 200; ++i) {
            store.set(static_cast<std::uint32_t>(i), CameraConfig{true, i, 30});
        }
    });
    writer.join();

    // Our snapshot did not change under us.
    assert(snap->size() == before);
    assert(snap->at(0).bitrate_kbps == 0);
    // The live map did.
    assert(store.snapshot()->size() == before + 100);
}

void test_concurrent_readers_writers() {
    SharedMutexConfigStore store;
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    threads.emplace_back([&] {
        for (int i = 0; i < scaled(5000, 200); ++i) {
            store.set(static_cast<std::uint32_t>(i % 32),
                      CameraConfig{true, i, 30});
        }
        stop.store(true, std::memory_order_release);
    });
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                std::vector<std::uint32_t> ids{1, 2, 3, 4, 5};
                auto v = store.get_many(ids);
                assert(v.size() == 5);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    test_basic<MutexConfigStore>();
    test_basic<SharedMutexConfigStore>();
    test_basic<CowConfigStore>();
    test_cow_snapshot_is_stable();
    test_concurrent_readers_writers();

    std::cout << "readers_writer_patterns: benchmark (read-mostly, 8 threads)\n";
    const int kReads = scaled(200000, 2000);
    bench<MutexConfigStore>("mutex       ", 8, kReads, 64);
    bench<SharedMutexConfigStore>("shared_mutex", 8, kReads, 64);
    bench<CowConfigStore>("cow         ", 8, kReads, 64);
    if (kSanitizerBuild) {
        std::cout << "  (sanitizer build: timings are instrumentation noise)\n";
    }

    std::cout << "readers_writer_patterns: ok\n";
    return 0;
}
