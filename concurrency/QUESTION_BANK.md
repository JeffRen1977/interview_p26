# Question Bank — Verkada C++ Concurrency

Practice out loud, on a timer. The 60-second answers matter more than you think:
most of an interview is talking, not typing.

---

## Part A — Rapid fire (answer in under 60 seconds each)

**1. Data race vs race condition?**
> Data race: two threads touch the same location, one writes, no happens-before,
> neither atomic — undefined behaviour. Race condition: outcome depends on
> timing; a design bug. You can have a race condition with no data race —
> check-then-act under a lock released in between.

**2. Why doesn't `volatile` make code thread-safe in C++?**
> It only stops the compiler eliding the access. No atomicity, no ordering,
> nothing about other cores' caches. It's for memory-mapped I/O. `std::atomic`
> is the thread-safety tool.

**3. `lock_guard` vs `unique_lock` vs `scoped_lock`?**
> `lock_guard`: RAII, no flexibility, cheapest. `unique_lock`: movable, can
> unlock early/defer/try — required for condition variables. `scoped_lock`
> (C++17): multiple mutexes at once, deadlock-free acquisition.

**4. Why must `cv.wait` always take a predicate?**
> Spurious wakeups are permitted by the standard, and a notify can arrive before
> you wait. The predicate re-checks the actual state, which is the real
> condition; the CV is just the wakeup mechanism.

**5. When does `shared_mutex` actually beat `mutex`?**
> When reads dominate *and* the read critical section is long enough to amortize
> the extra bookkeeping. For a single hash lookup it's usually slower. Measure.

**6. Four conditions for deadlock?**
> Mutual exclusion, hold-and-wait, no preemption, circular wait. Break any one.

**7. `notify_one` vs `notify_all`?**
> `notify_one` if all waiters wait on the same predicate and any one can proceed.
> `notify_all` if waiters have different predicates — otherwise you can wake the
> wrong thread and lose the wakeup entirely.

**8. Difference between `memory_order_relaxed` and `seq_cst`?**
> Relaxed: atomic, but no ordering vs other memory — fine for a counter you read
> after joining. seq_cst: a single total order across all threads, full barrier.
> Default, and always correct.

**9. What does `std::atomic<T>::is_lock_free()` returning false mean?**
> The implementation used a hidden lock. Your "lock-free" code isn't, and it may
> not be async-signal-safe. Always check for non-trivial `T`.

**10. Why can `++counter` lose updates but `fetch_add` can't?**
> `++` is load-modify-store — three steps, interleavable. `fetch_add` is a single
> indivisible RMW instruction.

**11. Is `std::shared_ptr` thread-safe?**
> The *control block* (refcount) is atomic, so copying the same `shared_ptr` from
> multiple threads is safe. The *pointee* is not protected at all, and writing
> the same `shared_ptr` object while another reads it is a data race — that
> needs `atomic<shared_ptr>` (C++20) or `atomic_load/store` (C++17).

**12. What's false sharing?**
> Two independent variables on the same cache line. Threads writing them fight
> over the line via the coherence protocol despite no logical sharing. Fix:
> `alignas(64)` / `hardware_destructive_interference_size`. Measurable — see
> `atomics_and_memory_order.cpp`.

**13. Why is double-checked locking with a raw pointer broken?**
> The publishing store can be reordered before the constructor's writes, so
> another thread sees non-null pointing at a half-built object. Fix: `atomic<T*>`
> with release/acquire — or just use a function-local static.

**14. How do you safely stop a worker thread?**
> `std::atomic<bool>` (or `std::stop_token` in C++20), set it, `notify_all` any
> CV the thread waits on, then `join`. Never `detach` and hope; never kill.

**15. What happens if `std::thread`'s destructor runs while joinable?**
> `std::terminate`. You must `join` or `detach` first — which is why you wrap it
> (`jthread` in C++20, or your own RAII type).

**16. Why not hold a lock while doing I/O?**
> You serialize every other thread behind an operation orders of magnitude
> slower than the critical section — and if the I/O can block indefinitely,
> you've built a liveness bug. Compute under the lock, act outside it.

**17. `steady_clock` vs `system_clock`?**
> `steady_clock` is monotonic — use it for every duration/timeout.
> `system_clock` is wall-clock and can jump backwards on NTP correction. Using
> it for a timeout can hang you for hours.

**18. What's a thundering herd, and how do you avoid it?**
> `notify_all` wakes every waiter; all but one immediately block again on the
> mutex. Use `notify_one` where the predicate is uniform, or separate CVs per
> condition (e.g. `not_empty` / `not_full`).

---

## Part B — Coding problems (whiteboard, 20–35 min each)

Ordered by likelihood for this interview.

### B1. Thread-safe bounded blocking queue ★★★★★
`bounded_blocking_queue.cpp`
Follow-ups: shutdown that wakes all waiters · timeout variants · multiple
producers/consumers · why two CVs instead of one · what if `T`'s move
constructor throws.

### B2. Thread pool with futures ★★★★★
`thread_pool.cpp`
Follow-ups: propagate exceptions to the caller · graceful shutdown, drain vs
discard · why `packaged_task` needs the `shared_ptr` wrapper · what if a task
submits another task (nested-submit deadlock) · per-thread work-stealing queues.

### B3. Real-time + historical event store ★★★★★
`timeseries_store.cpp` — **the one their prompt points at.**
Follow-ups: what if the flusher falls behind · how do you avoid duplicates at
the tier seam · how do you query across both · what's your bounded-memory
story · how would this shard across cameras.

### B4. Latest-frame buffer / frame dropping ★★★★
`producer_consumer_frame_dropping.cpp`
Follow-ups: why drop-oldest not drop-newest for live video · how do you avoid
tearing when the consumer is mid-read · how do you count drops.

### B5. Read-mostly config cache ★★★★
`readers_writer_patterns.cpp`
Follow-ups: `shared_mutex` vs COW vs double-buffer · when does COW lose ·
why can't `get()` return `const&`.

### B6. Thread-safe LRU cache ★★★★
`thread_safe_lru_cache.cpp`
Follow-ups: **why can't `get()` take a read lock?** (it mutates the list) ·
how do you get concurrent reads (CLOCK) · how do you reduce contention
(sharding, and what you lose) · add a TTL.

### B7. Rate limiter ★★★
`rate_limiter.cpp`
Follow-ups: token bucket vs sliding window · why lazy refill beats a timer
thread · make it lock-free · make it distributed.

### B8. SPSC lock-free ring buffer ★★★
`spsc_ring_buffer.cpp`
Follow-ups: exact memory orders and their pairing · why the sentinel slot ·
cache-line padding · why this doesn't generalize to MPMC.

### B9. Producer-consumer pipeline with fan-out ★★★
`local_sd_card_writer.cpp`
Follow-ups: differing consumer speeds · per-consumer drop policy · `shared_ptr`
for zero-copy fan-out · backpressure propagation.

### B10. Classics worth 10 minutes each
- **Dining philosophers** — lock ordering, or a waiter/arbitrator.
- **Print FooBar alternately / print in order** — CV + turn variable. Tests
  whether you can do the basics cleanly under pressure.
- **Read-write lock, implemented by hand** — from `mutex` + CV. Ask which
  side you should starve, and say writer-preference to avoid writer starvation.
- **Concurrent singleton** — magic static; `call_once`; correct DCLP.
- **H2O / semaphore-style barrier problems** — `counting_semaphore` (C++20) or
  CV + counter.

---

## Part C — Design/discussion questions

**C1. "A camera produces 30fps. Analytics takes 50ms/frame. What happens, and
what do you do?"**
> Consumer is slower than the producer — the queue grows without bound and you
> OOM. Options: bound the queue and drop frames (right for live analytics — you
> want the *latest* frame, not a backlog), scale consumers with a thread pool
> (if the work parallelizes), or apply backpressure and reduce capture rate. For
> analytics I'd drop-oldest and export a drop counter. For *recording* I'd never
> drop — I'd block or spill to disk, since losing recorded footage is a product
> failure, not a performance one. Note that these two paths need different
> policies fed by the same source, which is why you fan out with `shared_ptr`
> rather than sharing one queue.

**C2. "Users query 'all motion events on camera 7 last Tuesday' while 200
cameras are still writing. Design it."**
> See §4 of the study guide: hot ring under a plain mutex for ingest, cold
> indexed archive under `shared_mutex` for queries, single background flusher
> between them, query fans out to both and merges. Key points: ingest never
> touches the cold tier or disk; long queries can't block ingest; bounded hot
> memory; dedupe at the seam; drop counter exposed. Then talk partitioning by
> camera and time, and immutable time-partitioned segments so writers never
> touch what readers scan.

**C3. "How would you find a data race that only shows up in production?"**
> First: testing can't prove absence — a race can produce correct output every
> run. So (1) build under TSan and run the stress suite; it's a happens-before
> detector and flags the race even on a "passing" run. (2) Build a repro that
> reliably reproduces — more threads than cores, `sched_yield` in suspicious
> windows, TSan's `history_size`. (3) Read the code for the invariant: which
> lock protects which invariant, and where is it accessed without that lock.
> (4) In production: core dumps, and logging the state transitions. Also check
> the "obvious" places — non-atomic flags, `shared_ptr` aliasing, callbacks
> invoked under a lock, and any place a reference to protected data escapes.

**C4. "This class has all-atomic members. Is it thread-safe?"**
> No, and this is the most common misconception. Atomic members make each
> *individual* operation race-free, but invariants spanning two members are
> still violable — a reader can see `count_` updated and `total_` not. Locks
> protect *invariants*, not variables. Fix: one lock over the whole invariant,
> or pack the state into a single atomic. See `data_race_demos.cpp` race 3.

**C5. "How do you decide between a mutex and lock-free?"**
> Default to the mutex; an uncontended one is ~20ns and profoundly easier to get
> right. Go lock-free only with a measurement showing lock contention is the
> bottleneck, and only where the structure is simple enough to reason about —
> SPSC ring, counter, flag. Beyond that you're into ABA and safe memory
> reclamation (hazard pointers, epochs), which is a large correctness budget for
> usually-modest gains. Also check `is_lock_free()` — otherwise you've bought a
> hidden lock.

**C6. "Your queue's consumer crashed. What happens to the producer?"**
> With a bounded blocking queue: it fills, and the producer blocks forever — a
> liveness failure that looks like a hang, not a crash, which is worse to debug.
> Mitigations: `put` with a timeout so the producer notices, a health check on
> consumers, a shutdown flag every wait predicate checks, and a watchdog. This
> is why every blocking wait in production code should have a timeout or a
> cancellation path.

---

## Part D — Two-day plan (Sat 22nd → Mon 24th)

**Saturday**
1. `cmake -S . -B build && cmake --build build && ctest --test-dir build` — see
   it all pass. (30 min)
2. Read `STUDY_GUIDE.md` §1–3 aloud. Definitions must be automatic. (45 min)
3. **Write `thread_pool.cpp` from a blank file, on a timer.** No peeking.
   Compile it. Repeat until it's clean in under 20 minutes. (90 min)
4. Read `timeseries_store.cpp` end to end, including the comments about the
   bugs. Be able to draw the two-tier diagram from memory. (45 min)

**Sunday**
1. **Write a bounded blocking queue from scratch, timed.** Then add shutdown.
   (45 min)
2. Part A rapid fire — out loud, no notes. Anything you stumble on, re-read and
   redo. (45 min)
3. Read `atomics_and_memory_order.cpp` and `data_race_demos.cpp`. Be able to
   explain each race and its fix. (60 min)
4. Part C design questions — answer each out loud for 3–5 minutes as if to an
   interviewer. Record yourself if you can stand it. (45 min)
5. Run the TSan build; look at an actual race report:
   `./build-tsan/data_race_demos --broken`. (20 min)

**Monday morning**
- Skim `CHEATSHEET.md` only. No new material.
- Re-read §7 of the study guide (the war stories) — that's your differentiator.
- Have `bounded_blocking_queue.cpp` and `thread_pool.cpp` skeletons fresh.
