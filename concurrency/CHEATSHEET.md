# Concurrency Cheat Sheet — Monday morning skim only

## Definitions (must be automatic)

- **Data race** — same location, ≥1 write, no happens-before, not atomic. **UB.**
- **Race condition** — outcome depends on timing. Broader; can exist with no data race.
- **Happens-before** — from: unlock→lock (same mutex), release→acquire (same
  atomic), thread start, `join()`, `promise::set_value`→`future::get()`.
- **Locks protect invariants, not variables.**

## Skeletons

```cpp
// Bounded blocking queue — the one to have in muscle memory
template <typename T>
class BoundedQueue {
  std::queue<T> q_;  const size_t cap_;
  mutable std::mutex m_;
  std::condition_variable not_empty_, not_full_;
  bool stop_ = false;
 public:
  explicit BoundedQueue(size_t cap) : cap_(cap) {}

  bool push(T v) {
    std::unique_lock lk(m_);
    not_full_.wait(lk, [&]{ return q_.size() < cap_ || stop_; });
    if (stop_) return false;
    q_.push(std::move(v));
    lk.unlock();                 // notify outside the lock
    not_empty_.notify_one();
    return true;
  }

  std::optional<T> pop() {
    std::unique_lock lk(m_);
    not_empty_.wait(lk, [&]{ return !q_.empty() || stop_; });
    if (q_.empty()) return std::nullopt;   // stopped and drained
    T v = std::move(q_.front()); q_.pop();
    lk.unlock();
    not_full_.notify_one();
    return v;
  }

  void shutdown() {
    { std::lock_guard lk(m_); stop_ = true; }
    not_empty_.notify_all(); not_full_.notify_all();
  }
};
```

```cpp
// Thread pool — core loop
void worker() {
  for (;;) {
    std::function<void()> task;
    { std::unique_lock lk(m_);
      cv_.wait(lk, [&]{ return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) return;      // drain policy
      task = std::move(tasks_.front()); tasks_.pop(); }
    task();     // NEVER under the lock
  }
}

template <class F, class... A>
auto submit(F&& f, A&&... a) -> std::future<std::invoke_result_t<F, A...>> {
  using R = std::invoke_result_t<F, A...>;
  auto t = std::make_shared<std::packaged_task<R()>>(
      std::bind(std::forward<F>(f), std::forward<A>(a)...));
  auto fut = t->get_future();
  { std::lock_guard lk(m_); tasks_.emplace([t]{ (*t)(); }); }
  cv_.notify_one();
  return fut;
}
```

```cpp
// Acquire/release publish — the canonical pattern
// producer:                      // consumer:
payload = ...;                    while (!ready.load(acquire)) {}
ready.store(true, release);       use(payload);   // safe, no data race
```

```cpp
// CAS loop — recompute the new value from `expected` every iteration
auto cur = state.load(std::memory_order_relaxed);
while (!state.compare_exchange_weak(cur, next_from(cur),
        std::memory_order_acq_rel, std::memory_order_acquire)) { }
// CAS refreshes `cur` on failure. `_weak` is fine (and cheaper) in a loop.
```

```cpp
std::scoped_lock lk(m1, m2);          // multi-lock, deadlock-free (C++17)
static Singleton s;                    // thread-safe lazy init, C++11+
std::call_once(flag, []{ ... });       // the other one-time-init tool
```

## Rules

1. `cv.wait` **always** with a predicate.
2. Never hold a lock across I/O, sleep, or a user callback.
3. Never return `T&` / iterators to protected data. Return by value.
4. One global lock ordering, written down.
5. Bound every queue. Choose and *state* the overflow policy.
6. `steady_clock` for durations. Never `system_clock`.
7. Every blocking wait needs a shutdown path.
8. Count and expose your drops.

## Choosing

| Situation | Answer |
|---|---|
| Default | `std::mutex` + `lock_guard` |
| Two+ mutexes | `std::scoped_lock` |
| Read-mostly, **long** reads | `std::shared_mutex` |
| Read-mostly, tiny immutable data | COW: `shared_ptr<const T>` + atomic swap |
| Map contention | Shard by `hash(key) % N` |
| Counter / flag | `std::atomic` |
| Waiting on state | `condition_variable` + predicate |
| Single producer, single consumer | Lock-free ring, acquire/release |
| MPMC | Mutex. Seriously. |

## Memory order

`relaxed` — atomicity only (counters) ·
`acquire`/`release` — the publish pair ·
`acq_rel` — RMW that consumes and publishes ·
`seq_cst` — default, total order, always correct.

`volatile` ≠ atomic. Not a concurrency tool in C++.

## Say these unprompted

- "Let me get it correct first, then optimize."
- "An uncontended mutex is ~20ns — I'd measure before going lock-free."
- "Testing can't prove the absence of a race; I'd run this under TSan."
- "What's the overflow policy — block, drop-oldest, or drop-newest?"
- "This needs `steady_clock`; `system_clock` can jump backwards."
- "Atomic members don't make the class thread-safe if the invariant spans two."
- "`get()` mutates the LRU list, so it can't take a read lock."

## Ask before coding

Producers/consumers? · Bounded? · Overflow policy? · Latency requirements? ·
Read/write ratio? · How does shutdown work?

## Build

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan -j && ./build-tsan/data_race_demos --broken
```
