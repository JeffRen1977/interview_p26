// Atomics, memory ordering, and the primitives built from them.
//
// Verkada's prompt says "avoiding data races". The precise definition matters
// and you should be able to state it:
//
//   A DATA RACE is: two threads access the same memory location, at least one
//   access is a write, they are not ordered by a happens-before relationship,
//   and neither is atomic. A data race is UNDEFINED BEHAVIOUR in C++ -- not
//   "you get a stale value", but "the compiler may do anything", including
//   deleting your loop or hoisting the load out of it.
//
//   A RACE CONDITION is a broader design bug: the outcome depends on timing.
//   You can have a race condition with zero data races (e.g. check-then-act
//   under a mutex, released between the check and the act).
//
// Say that distinction out loud. It is the single fastest way to signal you
// actually know this material rather than having memorized "use a mutex".
//
// -------------------------------------------------------------------------
// The memory-order ladder -- what to say when asked:
//
//   relaxed : atomicity only, NO ordering vs other memory. Correct for a
//             statistics counter you only read after joining. Wrong for
//             anything that publishes data.
//
//   acquire : a load; nothing after it in program order can be reordered
//             before it. Pairs with release.
//   release : a store; nothing before it can be reordered after it. Everything
//             the releasing thread wrote before the store is visible to a
//             thread that acquire-loads that value.
//             -> This pair is the workhorse. "Write the data, release-store
//                the flag; acquire-load the flag, read the data."
//
//   acq_rel : for read-modify-write ops that both consume and publish.
//   seq_cst : default. Adds a single total order over all seq_cst ops across
//             all threads. Needed for things like Dekker/Peterson-style
//             algorithms and the classic store-buffer litmus test. Costs a
//             full barrier on x86 stores and dmb ish on ARM.
//
// Rule of thumb to state: start at seq_cst (the default, and always correct);
// weaken only where you have measured a win AND can name the pairing.

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "bench_scale.h"

// ---------------------------------------------------------------------------
// 1. Why `volatile` is NOT the answer (a real interview trap).
// ---------------------------------------------------------------------------
// volatile means "do not optimize away this access" -- it is for
// memory-mapped I/O registers. It provides NO atomicity and NO ordering
// guarantees between threads. In C++, the thread-safety tool is std::atomic.
// (Java/C# volatile is a different thing; don't let that confuse you.)

// ---------------------------------------------------------------------------
// 2. Relaxed counter: correct use of memory_order_relaxed.
// ---------------------------------------------------------------------------
// Frames-dropped statistics. We only need the increments not to be lost; we
// don't need them ordered against anything else. The final read happens after
// join(), which is itself a synchronization point.
void test_relaxed_counter() {
    std::atomic<std::uint64_t> frames_dropped{0};
    constexpr int kThreads = 8;
    const int kPerThread = scaled(100000);

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kPerThread; ++j) {
                frames_dropped.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) {
        t.join();  // join() gives happens-before: the load below is safe.
    }
    assert(frames_dropped.load(std::memory_order_relaxed) ==
           static_cast<std::uint64_t>(kThreads) * kPerThread);
}

// ---------------------------------------------------------------------------
// 3. Acquire/release: publishing a payload with a flag.
// ---------------------------------------------------------------------------
// THE canonical pattern. `payload` is a plain non-atomic struct, yet there is
// no data race, because the release/acquire pair creates happens-before.
struct FrameHeader {
    std::uint64_t sequence = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

void test_acquire_release_publish() {
    for (int iter = 0; iter < scaled(200, 20); ++iter) {
        FrameHeader payload;            // plain data, NOT atomic
        std::atomic<bool> ready{false};  // the flag IS atomic

        std::thread producer([&] {
            payload.sequence = 42;
            payload.width = 1920;
            payload.height = 1080;
            // release: everything above is visible to whoever acquires `ready`.
            ready.store(true, std::memory_order_release);
        });

        std::thread consumer([&] {
            // acquire: once we see true, we also see the writes above.
            while (!ready.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            assert(payload.sequence == 42);
            assert(payload.width == 1920);
            assert(payload.height == 1080);
        });

        producer.join();
        consumer.join();
    }
}

// ---------------------------------------------------------------------------
// 4. Spinlock from atomic_flag -- shows you understand CAS and contention.
// ---------------------------------------------------------------------------
// Use a spinlock ONLY when the critical section is a handful of instructions
// and you will not be preempted holding it. Otherwise a mutex (which parks the
// thread) is strictly better. On a camera SoC with fewer cores than threads,
// spinning is actively harmful.
class Spinlock {
 public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Test-and-test-and-set: spin on a plain load first. A bare
            // test_and_set loop writes the cache line every iteration and
            // ping-pongs it between cores.
#if defined(__cpp_lib_atomic_flag_test) && __cpp_lib_atomic_flag_test >= 201907L
            while (flag_.test(std::memory_order_relaxed)) {
                cpu_relax();
            }
#else
            cpu_relax();
#endif
        }
    }

    bool try_lock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() { flag_.clear(std::memory_order_release); }

 private:
    static void cpu_relax() {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();  // PAUSE: hints the CPU we are spinning
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

void test_spinlock() {
    Spinlock lock;
    std::uint64_t counter = 0;  // deliberately NOT atomic -- lock protects it
    constexpr int kThreads = 8;
    const int kPerThread = scaled(20000);

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kPerThread; ++j) {
                std::lock_guard<Spinlock> guard(lock);  // works: has lock/unlock
                ++counter;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    assert(counter == static_cast<std::uint64_t>(kThreads) * kPerThread);
}

// ---------------------------------------------------------------------------
// 5. compare_exchange: atomic max, and the weak-vs-strong distinction.
// ---------------------------------------------------------------------------
// compare_exchange_weak may fail SPURIOUSLY (it maps to LL/SC on ARM, which
// can fail if the reservation is lost). That is fine and cheaper INSIDE a
// retry loop -- which you need anyway. Use _strong only when you are not
// looping.
//
// Note: CAS updates `expected` in place on failure, so the loop needs no
// explicit reload.
void atomic_max(std::atomic<std::uint64_t>& target, std::uint64_t value) {
    std::uint64_t prev = target.load(std::memory_order_relaxed);
    while (prev < value &&
           !target.compare_exchange_weak(prev, value,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
        // `prev` was refreshed by the failed CAS; loop re-checks the condition.
    }
}

void test_atomic_max() {
    const int kPer = scaled(10000);
    std::atomic<std::uint64_t> high_water{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kPer; ++i) {
                atomic_max(high_water,
                           static_cast<std::uint64_t>(t * kPer + i));
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    assert(high_water.load() == static_cast<std::uint64_t>(8 * kPer - 1));
}

// ---------------------------------------------------------------------------
// 6. Correct lazy initialization -- and why the "clever" version is broken.
// ---------------------------------------------------------------------------
// Classic double-checked locking with a NON-atomic pointer is a textbook data
// race: the write that publishes the pointer can be reordered before the
// object's constructor finishes, so another thread sees a non-null pointer to
// a half-built object.
//
// Two correct answers, in order of preference:
//   (a) a function-local static -- the compiler emits a thread-safe guard for
//       you ("magic statics", guaranteed since C++11). This is the right
//       answer 95% of the time.
//   (b) std::call_once + std::once_flag.
//   (c) DCLP with std::atomic<T*> and acquire/release -- correct, but only
//       reach for it if you can justify it.
struct ExpensiveCodec {
    ExpensiveCodec() { ++construction_count; }
    static std::atomic<int> construction_count;
    int decode(int x) const { return x * 2; }
};
std::atomic<int> ExpensiveCodec::construction_count{0};

// (a) magic static
const ExpensiveCodec& codec_magic_static() {
    static ExpensiveCodec instance;  // thread-safe initialization, guaranteed
    return instance;
}

// (b) call_once
ExpensiveCodec* g_codec = nullptr;
std::once_flag g_codec_once;
const ExpensiveCodec& codec_call_once() {
    std::call_once(g_codec_once, [] { g_codec = new ExpensiveCodec(); });
    return *g_codec;
}

// (c) correct DCLP -- note the atomic pointer and the explicit orderings.
std::atomic<ExpensiveCodec*> g_dclp{nullptr};
std::mutex g_dclp_mutex;
const ExpensiveCodec& codec_dclp() {
    ExpensiveCodec* p = g_dclp.load(std::memory_order_acquire);
    if (p == nullptr) {
        std::lock_guard<std::mutex> lock(g_dclp_mutex);
        p = g_dclp.load(std::memory_order_relaxed);  // re-check under the lock
        if (p == nullptr) {
            p = new ExpensiveCodec();
            // release: the constructor's writes happen-before this store, so
            // an acquiring reader can never see a half-built object.
            g_dclp.store(p, std::memory_order_release);
        }
    }
    return *p;
}

void test_lazy_init() {
    const int before = ExpensiveCodec::construction_count.load();
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([] {
            assert(codec_magic_static().decode(21) == 42);
            assert(codec_call_once().decode(21) == 42);
            assert(codec_dclp().decode(21) == 42);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    // Exactly three objects were built, no matter how many threads raced.
    assert(ExpensiveCodec::construction_count.load() == before + 3);
}

// ---------------------------------------------------------------------------
// 7. False sharing -- measurable, and a favourite follow-up.
// ---------------------------------------------------------------------------
// Two atomics in the same 64-byte cache line make independent threads fight
// over that line. Padding to a cache line fixes it. Expect a several-x delta.
struct PackedCounters {
    std::atomic<std::uint64_t> a{0};
    std::atomic<std::uint64_t> b{0};  // same cache line as `a`
};

struct PaddedCounters {
    alignas(64) std::atomic<std::uint64_t> a{0};
    alignas(64) std::atomic<std::uint64_t> b{0};  // its own line
};

template <typename Counters>
double bench_false_sharing(const char* name) {
    Counters c;
    const int kIters = scaled(4000000);
    const auto start = std::chrono::steady_clock::now();
    std::thread t1([&] {
        for (int i = 0; i < kIters; ++i) {
            c.a.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::thread t2([&] {
        for (int i = 0; i < kIters; ++i) {
            c.b.fetch_add(1, std::memory_order_relaxed);
        }
    });
    t1.join();
    t2.join();
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    std::cout << "  " << name << ": " << ms << " ms\n";
    assert(c.a.load() == static_cast<std::uint64_t>(kIters) &&
           c.b.load() == static_cast<std::uint64_t>(kIters));
    return ms;
}

int main() {
    test_relaxed_counter();
    test_acquire_release_publish();
    test_spinlock();
    test_atomic_max();
    test_lazy_init();

    std::cout << "atomics_and_memory_order: false sharing benchmark\n";
    bench_false_sharing<PackedCounters>("packed (same line)");
    bench_false_sharing<PaddedCounters>("padded (own line) ");

    std::cout << "atomics_and_memory_order: ok\n";
    return 0;
}
