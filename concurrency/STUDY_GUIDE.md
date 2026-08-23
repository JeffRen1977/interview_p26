# Verkada C++ Concurrency Interview — Study Guide

**Interview: Monday 2026-08-24.** Their stated focus:

> thread safety, mutexes, and avoiding data races — as well as strategies for
> managing both real-time and historical data in multi-threaded environments

That last clause is the tell. Verkada builds cameras and cloud video. Expect a
problem shaped like *"frames/events arrive continuously; queries ask about both
the last few seconds and the last few weeks; design it and make it thread-safe."*
See `timeseries_store.cpp` — it is written to be exactly that answer.

---

## 1. The vocabulary that gets you taken seriously

Get these definitions exactly right. Precision here is the fastest signal that
you have done real concurrent work.

**Data race** — Two threads access the same memory location, at least one
access is a write, they are not ordered by a happens-before relationship, and
neither access is atomic. **A data race is undefined behaviour in C++.** Not
"you read a stale value" — the compiler is entitled to assume it cannot happen
and optimize accordingly, which is how a non-atomic `bool stop` flag becomes an
infinite loop.

**Race condition** — A broader design bug: the result depends on timing. You can
have a race condition with *zero* data races. `if (!cache.contains(k))
cache.insert(k,v)` — both calls perfectly locked, still broken, because the lock
is released in between.

> Say both definitions and the distinction. It takes ten seconds and it
> immediately separates you from candidates who only know "use a mutex."

**Thread-safe** — Correct when called concurrently from multiple threads with no
external synchronization. Note it is not binary: a class can be safe for
concurrent *reads* but not concurrent read+write (this is exactly the guarantee
the standard library gives you for containers).

**Reentrant vs thread-safe** — Different things. Reentrant = safe to re-enter
(recursion, signal handlers). A function using a static buffer is thread-safe
with a lock but still not reentrant.

**Atomicity / Visibility / Ordering** — The three separate guarantees. `atomic`
gives all three; `volatile` gives *none* of them. Don't conflate them.

**Happens-before** — The ordering relation that makes concurrent access legal.
Established by: unlock→lock on the same mutex, release-store→acquire-load of the
same atomic, thread creation, `join()`, and `future::get()` vs the promise.

**Invariant** — The property spanning multiple variables that your lock actually
protects. Locks don't protect *variables*; they protect *invariants*. This
sentence is the whole reason `atomic<int> count_` + `atomic<long> total_` is
still a broken class.

---

## 2. Mutex family — what to reach for

| Primitive | Use when |
|---|---|
| `std::mutex` | Default. Reach for this first, always. |
| `std::lock_guard` | Simple scoped lock. Cheapest, no flexibility. |
| `std::unique_lock` | Need to unlock early, defer, `try_to_lock`, or use a CV. |
| `std::scoped_lock` (C++17) | Locking 2+ mutexes at once. Deadlock-free by construction. |
| `std::shared_mutex` (C++17) | Read-mostly **and** a long read critical section. |
| `std::recursive_mutex` | Almost never. Usually signals a design problem — split into `public` + `_locked` private instead. |
| `std::atomic` | Single-variable state, counters, flags. |
| `std::condition_variable` | Waiting for a *state change*, not for a lock. |
| `std::call_once` / magic static | One-time initialization. |
| Spinlock | Critical section is a few instructions, and you won't be preempted. Rarely on a camera SoC. |

### Non-negotiable rules

1. **Always wait with a predicate.** `cv.wait(lock, [&]{ return ready; })`, never
   a bare `if`. **Spurious wakeups are real** and permitted by the standard.
2. **Never hold a lock across a blocking call** — no I/O, no `sleep`, no
   callback into user code, no waiting on another thread that might want your
   lock. Compute under the lock, act outside it.
3. **Never return a reference or iterator to protected data.** The lock dies at
   the end of the function; the reference outlives it. Return by value, or
   return a `shared_ptr` snapshot.
4. **One lock ordering, documented, globally.** If the order is written down and
   obeyed, circular wait is impossible.
5. **`notify` outside the lock when you can** — otherwise the woken thread
   immediately blocks on the mutex you still hold ("hurry up and wait"). Minor,
   but correct, and interviewers notice.
6. **Prefer `notify_one`** unless waiters wait on different predicates — then you
   *must* `notify_all` or you can lose a wakeup.

---

## 3. The memory-order ladder

Start at `seq_cst` (the default, always correct). Weaken only when you can name
the pairing.

- **`relaxed`** — atomicity only, no ordering. Correct for a statistics counter
  read after `join()`. Wrong for anything that publishes data.
- **`acquire` / `release`** — the workhorse pair. *Write the data, release-store
  the flag; acquire-load the flag, then read the data.* Everything the releasing
  thread wrote before the store is visible to the acquiring thread.
- **`acq_rel`** — read-modify-write ops that both consume and publish.
- **`seq_cst`** — adds a single total order across all threads. Needed for
  store-buffer / Dekker-style algorithms.

**`volatile` is not a concurrency tool in C++.** It's for memory-mapped I/O. No
atomicity, no ordering. (Java's `volatile` is a different beast — don't let that
confuse you.)

---

## 4. Real-time + historical data — the design answer

This is the part of the prompt you must nail. The framework:

**Split the data by temperature; give each tier the primitive that matches its
access pattern.**

| Tier | Contents | Access pattern | Primitive |
|---|---|---|---|
| **Hot** | last N seconds, fixed capacity | high-frequency append, latency-critical | plain `mutex` — critical section is a few instructions |
| **Cold** | full history, indexed | bursty range queries, read-dominated | `shared_mutex` — long critical section, many readers |
| **Bridge** | background flusher, hot → cold | single writer | serialize it; it is the only cold writer |

**Why not one big lock?** A long historical query would block the real-time
ingest path. That coupling is exactly what makes camera pipelines drop frames.
Tiering decouples query tail latency from ingest.

Points to raise unprompted — each one is a "this person has shipped this" signal:

- **`steady_clock`, never `system_clock`,** for anything measuring elapsed time.
  `system_clock` jumps backwards on NTP correction. On a camera that just synced
  time after boot, that is a real bug, not a theoretical one.
- **Bounded memory or you die.** The hot tier must be fixed-capacity. Unbounded
  queues turn a slow consumer into an OOM.
- **Choose your drop policy explicitly**: block the producer (backpressure),
  drop-oldest (keep freshness — right for live video), or drop-newest. Say which
  and why. For live preview: drop-oldest, you only want the latest frame. For
  recording: block or spill to disk, you cannot lose frames.
- **Count what you drop.** Expose the counter. Silent loss is the worst failure
  mode. (I hit exactly this in `timeseries_store.cpp` — see §7.)
- **Define ordering internally.** Don't trust a sequence number assigned by
  concurrent producers before they append; ids arrive out of order. Have the
  buffer assign its own sequence under its own lock.
- **The seam between tiers needs explicit handling** — dedupe or a watermark,
  and you should be able to say *why* the naive watermark isn't enough.

---

## 5. Scaling patterns, in the order you should propose them

1. **Don't share.** Thread-local accumulation, merge at the end. Always the
   fastest answer when it applies.
2. **Shard.** N independent locks keyed by `hash(key) % N`. The standard answer
   to map contention. Price: you lose any global ordering property.
3. **Reader-writer lock.** Only when the read critical section is long. For a
   one-line hash lookup, `shared_mutex` bookkeeping can cost more than the
   `mutex` it replaces — say this, it shows you've measured.
4. **Copy-on-write / RCU.** Lock-free reads, O(n) writes. For small,
   near-immutable data on a hot read path (config, routing tables).
5. **Lock-free with atomics.** Last resort. Justify it with a measurement, and
   mention the hazards: ABA, memory reclamation, and that `is_lock_free()` can
   be `false` — in which case the library took a hidden lock and you gained
   nothing.

**An uncontended `std::mutex` is ~20ns.** Most "we need lock-free" instincts are
wrong. Saying this out loud shows judgment, which is what senior interviews test.

### Numbers you measured yourself (Apple M-series, RelWithDebInfo)

Quote these. "I benchmarked it" beats "I read that" every time.

**Read-mostly config store, 8 reader threads, 1.6M reads, one slow writer**
(`readers_writer_patterns.cpp`):

| Strategy | Time |
|---|---|
| `std::mutex` | 54 ms |
| `std::shared_mutex` | **80 ms — slower!** |
| Copy-on-write | 46 ms |

> `shared_mutex` **lost to a plain mutex** here. The read critical section is a
> single hash lookup, so the reader-writer bookkeeping costs more than the
> contention it removes. This is the concrete answer to "wouldn't a read-write
> lock be faster?" — *"not necessarily, and here's a case where I measured it
> losing. It pays off when the read critical section is long."*

**False sharing, two threads incrementing two counters** 
(`atomics_and_memory_order.cpp`):

| Layout | Time |
|---|---|
| Both counters in one cache line | 35 ms |
| `alignas(64)` on each | **7 ms — 5x faster** |

> Same instructions, same logical work, no sharing at the language level — 5x
> from cache-line placement alone.

---

## 6. Deadlock

Four conditions — **mutual exclusion, hold-and-wait, no preemption, circular
wait.** Break any one and deadlock is impossible.

In practice:
- Break circular wait → **global lock ordering** (`deadlock_and_lock_discipline.cpp`).
- Break hold-and-wait → **`std::scoped_lock`** (locks all at once), or
  **try-lock + release + randomized backoff** (watch for livelock — mention it).
- Avoid self-deadlock → **split `public` (locks) from `private _locked`
  (assumes locked)**. This is a better answer than `recursive_mutex`.

---

## 7. What went wrong when I built this kit (use these stories)

Real bugs found while making the tests pass. Concrete war stories land far
better than recited theory, and every one of these is a plausible interview
follow-up.

1. **Watermark used producer-assigned ids.** Producers took ids from a shared
   atomic *then* appended, so thread A could take id 100, get descheduled, and
   append after B's id 101. "Everything ≤ N is flushed" was simply false, and
   event 100 was silently lost forever.
   **Lesson:** a component that needs an ordering must define that ordering
   itself, not trust one handed in by concurrent callers.

2. **Query raced the flusher.** Reading the watermark before vs. after the cold
   query gives you *duplicates* or *lost events* respectively — there is no
   ordering that gives neither. Chose the duplicate-producing order deliberately
   and added an explicit dedupe.
   **Lesson:** when you can't eliminate a failure mode, pick the one you can
   repair. Losing data is unacceptable; a duplicate is one `std::unique` away.

3. **Drop counter cried wolf.** It counted an overwrite as data loss even when
   the flusher had already copied the event out but not yet marked it flushed.
   Reported 802 drops while all 8000 events were safely stored.
   **Lesson:** a metric that fires falsely is worse than no metric. Track
   "captured" separately from "durable."

4. **"Only one thread flushes" was a comment, not a guarantee.** `flush_now()`
   was public, so a caller could drain concurrently with the background flusher
   and double-insert.
   **Lesson:** if the design says *only one thread does X*, make the code
   enforce it.

5. **`RelWithDebInfo` defines `NDEBUG`, silently deleting every `assert`.** The
   entire suite was a no-op until I stripped it in CMake.
   **Lesson:** verify your tests can actually fail.

---

## 8. Interview execution

**Before writing code, ask:**
- How many producers, how many consumers?
- Bounded or unbounded? What happens when full — block, drop, or grow?
- What are the latency requirements? Is any path real-time?
- Read/write ratio?
- How does shutdown work?

Then state your design in one sentence before you write it.

**While coding:**
- Write the simple mutex version first, correctly. Say *"let me get it correct,
  then we can talk about optimizing."* Nobody is annoyed by this; plenty of
  candidates die optimizing prematurely.
- Narrate the invariant each lock protects.
- Handle shutdown. Most candidates forget; it's often the follow-up.

**After coding, volunteer:**
- "Here's how I'd test this — and here's why testing can't prove the absence of
  a race, so I'd run it under TSan."
- "Here's what I'd measure before optimizing."
- "Here's the failure mode under overload."

**If you get stuck:** say what you're considering out loud. Silence reads as
being lost; thinking out loud reads as collaboration.

---

## 9. Files in this kit

Fundamentals:
- `thread_pool.cpp` — **most likely single question.** Futures, exception
  propagation, graceful shutdown, nested submit.
- `atomics_and_memory_order.cpp` — memory orders, spinlock, CAS loops, lazy
  init, measurable false sharing.
- `deadlock_and_lock_discipline.cpp` — three deadlock-free strategies.
- `data_race_demos.cpp` — five race archetypes, broken and fixed. Run with
  `--broken` under TSan.

Real-time + historical:
- `timeseries_store.cpp` — **the flagship.** Hot ring + cold archive + flusher.
- `readers_writer_patterns.cpp` — mutex vs `shared_mutex` vs COW, benchmarked.
- `thread_safe_lru_cache.cpp` — strict LRU, sharded LRU, CLOCK.
- `rate_limiter.cpp` — token bucket (mutex + lock-free), sliding window.

Queues & camera pipeline (pre-existing):
- `bounded_blocking_queue.cpp`, `thread_safe_queue.cpp`,
  `thread_safe_ring_buffer.cpp`, `spsc_ring_buffer.cpp`,
  `producer_consumer_frame_dropping.cpp`, `video_ring_buffer.cpp`,
  `local_sd_card_writer.cpp`
