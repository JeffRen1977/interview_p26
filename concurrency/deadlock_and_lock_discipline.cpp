// Deadlock: how to cause it, how to prove it can't happen, how to fix it.
//
// Interviewers ask "what are the four conditions for deadlock?" -- know them:
//   1. Mutual exclusion      -- the resource can't be shared.
//   2. Hold and wait         -- a thread holds one lock while waiting another.
//   3. No preemption         -- locks can't be forcibly taken away.
//   4. Circular wait         -- a cycle in the "waits-for" graph.
// Break ANY one of them and deadlock is impossible. In practice you break #4
// with a global lock ordering, or #2 with std::scoped_lock / try-lock+backoff.
//
// The canonical setup below is "transfer between two accounts", which in a
// camera product is "move a recording between two storage volumes" or "swap
// two entries in a device registry". Same shape.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "bench_scale.h"

// ---------------------------------------------------------------------------
// The resource: a storage volume with a byte budget.
// ---------------------------------------------------------------------------
class Volume {
 public:
    Volume(std::string name, std::int64_t bytes)
        : name_(std::move(name)), bytes_(bytes) {}

    std::mutex& mutex() { return mutex_; }
    std::int64_t bytes() const { return bytes_; }        // call under lock
    void add(std::int64_t n) { bytes_ += n; }            // call under lock
    const std::string& name() const { return name_; }
    // Stable identity for ordering. Address works; an explicit id is clearer
    // and survives relocation.
    std::uintptr_t id() const {
        return reinterpret_cast<std::uintptr_t>(this);
    }

 private:
    std::string name_;
    std::int64_t bytes_;
    std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// BROKEN: locks in argument order -> circular wait.
// ---------------------------------------------------------------------------
// transfer(A, B) locks A then B; transfer(B, A) locks B then A. Two threads
// doing both directions concurrently deadlock. This is NOT called by the tests
// -- it is here so you can point at the exact bug in an interview.
[[maybe_unused]] void transfer_BROKEN(Volume& from, Volume& to,
                                      std::int64_t n) {
    std::lock_guard<std::mutex> l1(from.mutex());
    std::this_thread::sleep_for(std::chrono::microseconds(1));  // widen window
    std::lock_guard<std::mutex> l2(to.mutex());  // <-- may already be held
    from.add(-n);
    to.add(n);
}

// ---------------------------------------------------------------------------
// FIX 1: std::scoped_lock (C++17) -- the answer you should give first.
// ---------------------------------------------------------------------------
// scoped_lock locks all mutexes with a deadlock-avoidance algorithm
// (try-lock in a rotating order, back off and retry on failure). It is also
// exception-safe and unlocks in reverse order.
//
// C++11 equivalent: std::lock(m1, m2) followed by two
// std::lock_guard(m, std::adopt_lock). Know both; say scoped_lock is the
// modern one-liner.
void transfer_scoped_lock(Volume& from, Volume& to, std::int64_t n) {
    if (&from == &to) {
        return;  // self-transfer: locking the same mutex twice is UB
    }
    std::scoped_lock lock(from.mutex(), to.mutex());  // atomic acquisition
    from.add(-n);
    to.add(n);
}

// ---------------------------------------------------------------------------
// FIX 2: explicit lock ordering -- break circular wait by hand.
// ---------------------------------------------------------------------------
// Establish a total order over the resources and ALWAYS acquire in that order.
// Use this when the locks are taken in different functions and scoped_lock
// can't see them all at once. It is also what you enforce in code review /
// with a lock-order checker.
void transfer_ordered(Volume& from, Volume& to, std::int64_t n) {
    if (&from == &to) {
        return;
    }
    Volume* first = &from;
    Volume* second = &to;
    if (first->id() > second->id()) {
        std::swap(first, second);  // canonical order, regardless of direction
    }
    std::lock_guard<std::mutex> l1(first->mutex());
    std::lock_guard<std::mutex> l2(second->mutex());
    from.add(-n);
    to.add(n);
}

// ---------------------------------------------------------------------------
// FIX 3: try_lock with backoff -- break "hold and wait".
// ---------------------------------------------------------------------------
// Grab the first lock; if the second isn't available, RELEASE the first and
// retry. Never deadlocks, but can livelock if every thread retries in
// lockstep -- so add randomized backoff. Mention livelock; it shows depth.
void transfer_try_lock(Volume& from, Volume& to, std::int64_t n) {
    if (&from == &to) {
        return;
    }
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> jitter(0, 50);

    for (;;) {
        std::unique_lock<std::mutex> l1(from.mutex());
        std::unique_lock<std::mutex> l2(to.mutex(), std::try_to_lock);
        if (l2.owns_lock()) {
            from.add(-n);
            to.add(n);
            return;
        }
        l1.unlock();  // release everything before retrying -- the key step
        std::this_thread::sleep_for(std::chrono::microseconds(jitter(rng)));
    }
}

// ---------------------------------------------------------------------------
// The other classic deadlock: recursive re-entry on a non-recursive mutex.
// ---------------------------------------------------------------------------
// A public method takes the lock, then calls another public method that takes
// the same lock -> self-deadlock (UB for std::mutex).
//
// The fix people reach for is std::recursive_mutex. The BETTER fix, and the
// answer that impresses: split into a locked public wrapper and an unlocked
// private `_locked` implementation. recursive_mutex usually hides a design
// problem -- you no longer know what invariants hold at any point.
class Registry {
 public:
    void add(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        add_locked(id);
    }

    void add_many(const std::vector<int>& ids) {
        std::lock_guard<std::mutex> lock(mutex_);  // taken ONCE
        for (int id : ids) {
            add_locked(id);  // no re-lock -- calls the unlocked helper
        }
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

 private:
    // Precondition: mutex_ is held by the caller. Encode that in the name.
    void add_locked(int id) { items_.push_back(id); }

    std::vector<int> items_;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// Tests: hammer transfers in BOTH directions. A deadlocking implementation
// would hang here rather than fail -- which is itself the lesson.
// ---------------------------------------------------------------------------
template <typename TransferFn>
void test_no_deadlock(const char* name, TransferFn transfer) {
    Volume a("volume-a", 100000);
    Volume b("volume-b", 100000);
    Volume c("volume-c", 100000);
    const std::int64_t total = a.bytes() + b.bytes() + c.bytes();

    constexpr int kThreads = 8;
    const int kIters = scaled(2000, 100);
    std::vector<Volume*> vols{&a, &b, &c};
    std::vector<std::thread> threads;

    const auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kIters; ++i) {
                // Deliberately opposing orders across threads.
                Volume* from = vols[static_cast<std::size_t>((t + i) % 3)];
                Volume* to = vols[static_cast<std::size_t>((t + i + 1) % 3)];
                if (t % 2 == 0) {
                    std::swap(from, to);
                }
                transfer(*from, *to, 1);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count();

    // Conservation: no bytes created or destroyed. This catches lost updates
    // (a torn read-modify-write), not just deadlock.
    assert(a.bytes() + b.bytes() + c.bytes() == total);
    std::cout << "  " << name << ": ok (" << ms << " ms)\n";
}

void test_registry_no_self_deadlock() {
    const int kIters = scaled(1000, 100);
    Registry r;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                r.add(i);
                r.add_many({i, i + 1, i + 2});
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(r.size() == static_cast<std::size_t>(4 * kIters * 4));
}

int main() {
    std::cout << "deadlock_and_lock_discipline: three deadlock-free strategies\n";
    test_no_deadlock("scoped_lock  ", transfer_scoped_lock);
    test_no_deadlock("lock ordering", transfer_ordered);
    test_no_deadlock("try_lock     ", transfer_try_lock);
    test_registry_no_self_deadlock();
    std::cout << "deadlock_and_lock_discipline: ok\n";
    return 0;
}
