// Data races: what they look like, why "it works on my machine" is not
// evidence, and how ThreadSanitizer finds them.
//
// Build the broken versions on purpose:
//     cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
//     cmake --build build-tsan
//     ./build-tsan/data_race_demos --broken
//
// Without --broken this binary runs only the FIXED versions, so it passes
// ctest and is clean under TSan. With --broken it runs the racy code so you
// can see TSan's report. That distinction is itself worth mentioning: you
// build a repro that reliably reproduces, then you let the tool localize it.
//
// Talking point: a data race is undefined behaviour, so testing cannot prove
// its absence. TSan is a happens-before race detector -- it flags a race even
// when the run produced the "right" answer, which is exactly what you need.

#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include "bench_scale.h"

// ===========================================================================
// Race 1: non-atomic read-modify-write.
// ===========================================================================
// `++counter` is load, add, store -- three steps. Two threads interleave and
// updates are lost. The bug is easy; what interviewers want is your
// explanation of WHY making it atomic is not always the answer (see Race 3).
void race1_broken() {
    constexpr int kThreads = 4;
    constexpr int kIters = 100000;
    int counter = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIters; ++j) {
                ++counter;  // RACE: unsynchronized read-modify-write
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    const int expected = kThreads * kIters;
    std::cout << "  race1 broken: counter=" << counter << " expected="
              << expected;
    if (counter == expected) {
        // This happens often in optimized builds -- the compiler keeps the
        // counter in a register for the whole loop, so the threads barely
        // interleave. THIS IS THE LESSON: the code is still undefined
        // behaviour, and TSan still reports it, even though the answer is
        // right. A green test run is not evidence of correctness here.
        std::cout << "  <-- CORRECT ANSWER, STILL BROKEN CODE\n";
    } else {
        std::cout << "  (lost " << (expected - counter) << " updates)\n";
    }
}

void race1_fixed() {
    const int kIters = scaled(100000, 1000);
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIters; ++j) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(counter.load() == 4 * kIters);
}

// ===========================================================================
// Race 2: the "it looks fine" flag. A non-atomic bool used to stop a thread.
// ===========================================================================
// This is the one that bites people in production. The compiler is allowed to
// hoist the load of `stop` out of the loop -- because in a race-free program
// nothing else could change it -- turning it into `if (!stop) for(;;) {}`.
// An infinite loop from a "harmless" bool.
struct BrokenStopFlag {
    bool stop = false;  // WRONG
    long long work = 0;
};

void race2_broken() {
    BrokenStopFlag s;
    std::thread worker([&] {
        while (!s.stop) {  // RACE: non-atomic load, may be hoisted
            ++s.work;
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    s.stop = true;  // RACE: non-atomic store
    worker.join();
    // Note what you actually observe: `work` is frequently 0 even though the
    // loop clearly ran for 20ms. The compiler is entitled to keep `work` in a
    // register and never write it back, because in a race-free program nothing
    // else could observe it. Same reasoning lets it hoist the `s.stop` load out
    // of the loop entirely -- on other compilers/flags this exact code becomes
    // an infinite loop and the process hangs.
    //
    // "It exited on my machine" is not a correctness argument.
    std::cout << "  race2 broken: work=" << s.work
              << " (loop ran ~20ms; a plausible value would be millions)\n";
}

void race2_fixed() {
    std::atomic<bool> stop{false};
    long long work = 0;
    std::thread worker([&] {
        while (!stop.load(std::memory_order_acquire)) {
            ++work;  // fine: only this thread touches `work` until join()
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    stop.store(true, std::memory_order_release);
    worker.join();
    assert(work > 0);
}

// ===========================================================================
// Race 3: atomic members do NOT make a class thread-safe.
// ===========================================================================
// The single best "gotcha" to bring up unprompted. Every member is atomic,
// every individual operation is race-free -- and the class is still broken,
// because the INVARIANT spans two members. This is a race condition without a
// data race.
class BrokenStats {
 public:
    void record(int latency_ms) {
        count_.fetch_add(1, std::memory_order_relaxed);
        total_.fetch_add(latency_ms, std::memory_order_relaxed);
        // Between these two lines another thread can observe count_ updated
        // and total_ not -> average() is wrong. No data race; still a bug.
    }
    double average() const {
        const int c = count_.load(std::memory_order_relaxed);
        const long long t = total_.load(std::memory_order_relaxed);
        return c == 0 ? 0.0 : static_cast<double>(t) / c;  // torn pair
    }

 private:
    std::atomic<int> count_{0};
    std::atomic<long long> total_{0};
};

// Fix: make the INVARIANT atomic, not the fields. One lock over the pair.
class FixedStats {
 public:
    void record(int latency_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        total_ += latency_ms;
    }
    double average() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0 ? 0.0 : static_cast<double>(total_) / count_;
    }

 private:
    int count_ = 0;
    long long total_ = 0;
    mutable std::mutex mutex_;
};

void race3_fixed() {
    FixedStats stats;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < scaled(10000, 500); ++j) {
                stats.record(10);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(stats.average() == 10.0);
}

// ===========================================================================
// Race 4: check-then-act. A race condition with no data race at all.
// ===========================================================================
// Each individual call is perfectly synchronized. The BUG is that the lock is
// released between the check and the act, so the answer goes stale.
class Cache {
 public:
    bool contains(const std::string& k) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.count(k) > 0;
    }
    void insert(const std::string& k, int v) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_[k] = v;
        ++insert_count_;
    }
    // The fix: one atomic operation that does the check AND the act.
    bool insert_if_absent(const std::string& k, int v) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (map_.count(k) > 0) {
            return false;
        }
        map_[k] = v;
        ++insert_count_;
        return true;
    }
    int insert_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return insert_count_;
    }

 private:
    std::map<std::string, int> map_;
    int insert_count_ = 0;
    mutable std::mutex mutex_;
};

void race4_broken() {
    Cache cache;
    std::vector<std::thread> threads;
    // Start all threads at once so they genuinely overlap; without this they
    // tend to run sequentially and the bug hides.
    std::atomic<bool> go{false};
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {
            }
            // TOCTOU: the lock is released when contains() returns, so another
            // thread can insert before we act on the answer. Both calls are
            // perfectly synchronized; the BOUNDARY between them is the bug.
            if (!cache.contains("key")) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                cache.insert("key", 1);
            }
        });
    }
    go.store(true, std::memory_order_release);
    for (auto& t : threads) {
        t.join();
    }
    std::cout << "  race4 broken: inserts=" << cache.insert_count()
              << " (wanted exactly 1)"
              << (cache.insert_count() > 1 ? "  <-- BUG REPRODUCED" : "")
              << "\n";
}

void race4_fixed() {
    Cache cache;
    std::atomic<int> winners{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            if (cache.insert_if_absent("key", 1)) {
                winners.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(cache.insert_count() == 1);
    assert(winners.load() == 1);  // exactly one thread wins
}

// ===========================================================================
// Race 5: iterator invalidation across threads.
// ===========================================================================
// Reading a container while another thread mutates it is a data race AND a
// dangling-pointer bug: rehash/reallocation invalidates iterators and
// references. A shared_lock prevents it -- but only if EVERY writer takes the
// unique_lock. One unguarded write anywhere ruins the whole scheme.
class SafeIndex {
 public:
    void add(int k, const std::string& v) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_[k] = v;
    }
    // Return by VALUE. Returning const std::string& would let the caller hold
    // a reference after the lock is dropped -- a dangling read on the next
    // rehash.
    std::string get(int k) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(k);
        return it == map_.end() ? std::string{} : it->second;
    }
    std::size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_.size();
    }

 private:
    std::map<int, std::string> map_;
    mutable std::shared_mutex mutex_;
};

void race5_fixed() {
    const int kAdds = scaled(5000, 300);
    SafeIndex index;
    std::vector<std::thread> threads;
    threads.emplace_back([&] {
        for (int i = 0; i < kAdds; ++i) {
            index.add(i, "camera-" + std::to_string(i));
        }
    });
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kAdds; ++i) {
                auto s = index.get(i % 100);
                (void)s;
                (void)s;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(index.size() == static_cast<std::size_t>(kAdds));
}

int main(int argc, char** argv) {
    const bool run_broken = (argc > 1 && std::strcmp(argv[1], "--broken") == 0);

    race1_fixed();
    race2_fixed();
    race3_fixed();
    race4_fixed();
    race5_fixed();

    if (run_broken) {
        std::cout << "\n--- running RACY versions (expect TSan reports) ---\n";
        race1_broken();  // TSan reports a data race
        race2_broken();  // TSan reports a data race
        race4_broken();  // TSan reports NOTHING -- see below
        std::cout << "--- end racy section ---\n\n";

        // THE MOST IMPORTANT LINE IN THIS FILE:
        //
        // Under TSan you get exactly TWO race reports for THREE broken demos.
        // race4 is silent, because check-then-act has no data race -- every
        // access is properly locked. It is a race condition in the DESIGN, and
        // no sanitizer can find it, because nothing about it is illegal at the
        // memory-model level. It just computes the wrong answer.
        //
        // The takeaway to state in an interview: TSan is necessary but not
        // sufficient. It finds unsynchronized access; it cannot find a wrong
        // synchronization boundary. For that you need to reason about which
        // invariant the lock protects and whether it is ever observed
        // mid-update. Tools check the memory model; only you check the design.
        std::cout << "note: race4 produces NO TSan report -- it is a race\n"
                     "      condition, not a data race. Tools cannot find it.\n\n";
    }

    std::cout << "data_race_demos: ok"
              << (run_broken ? " (broken demos also ran)" : "") << "\n";
    return 0;
}
