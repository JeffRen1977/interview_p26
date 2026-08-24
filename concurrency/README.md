# Concurrency interview kit

C++ concurrency problems with runnable, tested implementations. Built for a
**Verkada C++ concurrency interview (Mon 2026-08-24)**, whose stated focus was
thread safety, mutexes, avoiding data races, and *"managing both real-time and
historical data in multi-threaded environments."*

## Start here

| Document | Use |
|---|---|
| [CHAPTER_CONCURRENCY.md](./CHAPTER_CONCURRENCY.md) | Book chapter draft — teaching prose built on these examples |
| [STUDY_GUIDE.md](./STUDY_GUIDE.md) | Concepts, the two-tier real-time/historical design, war stories |
| [QUESTION_BANK.md](./QUESTION_BANK.md) | 18 rapid-fire Qs, 10 coding problems, 6 design Qs, 2-day plan |
| [CHEATSHEET.md](./CHEATSHEET.md) | Morning-of skim: skeletons, rules, decision table |
| [读多写少.md](./读多写少.md) | Read-mostly patterns in C++/Go/Java |

## Problems

### Fundamentals
| File | Problem | Key idea |
|------|---------|----------|
| [thread_pool.cpp](./thread_pool.cpp) | Fixed-size pool + futures | `packaged_task`; exception propagation; drain-on-shutdown; nested submit |
| [atomics_and_memory_order.cpp](./atomics_and_memory_order.cpp) | Atomics from the ground up | relaxed/acquire-release/seq_cst; spinlock; CAS loop; lazy init; false sharing (measured) |
| [deadlock_and_lock_discipline.cpp](./deadlock_and_lock_discipline.cpp) | Deadlock, 3 ways to avoid it | `scoped_lock`; global lock ordering; try-lock + backoff; `public`/`_locked` split |
| [data_race_demos.cpp](./data_race_demos.cpp) | 5 race archetypes, broken + fixed | Run `--broken` under TSan; **race 4 is invisible to TSan** |

### Real-time + historical data
| File | Problem | Key idea |
|------|---------|----------|
| [timeseries_store.cpp](./timeseries_store.cpp) | **Unified live + historical event store** | Hot ring (mutex) + cold archive (`shared_mutex`) + background flusher; seam dedupe; drop accounting |
| [readers_writer_patterns.cpp](./readers_writer_patterns.cpp) | Read-mostly config | mutex vs `shared_mutex` vs COW, benchmarked |
| [thread_safe_lru_cache.cpp](./thread_safe_lru_cache.cpp) | Thread-safe LRU | Why `get()` can't take a read lock; sharding; CLOCK for concurrent reads |
| [rate_limiter.cpp](./rate_limiter.cpp) | Throttling | Token bucket (mutex + lock-free CAS); sliding window; `steady_clock` |

### Queues and buffers
| File | Problem | Key idea |
|------|---------|----------|
| [bounded_blocking_queue.cpp](./bounded_blocking_queue.cpp) | Bounded blocking queue | mutex + 2 CVs; put/get block |
| [thread_safe_queue.cpp](./thread_safe_queue.cpp) | Minimal bounded queue | whiteboard skeleton of the above |
| [thread_safe_ring_buffer.cpp](./thread_safe_ring_buffer.cpp) | MPMC ring (mutex) | circular buffer; non-blocking push/pop |
| [spsc_ring_buffer.cpp](./spsc_ring_buffer.cpp) | Lock-free SPSC | sentinel slot; cache-line padding |

### Camera pipeline
| File | Problem | Key idea |
|------|---------|----------|
| [producer_consumer_frame_dropping.cpp](./producer_consumer_frame_dropping.cpp) | Latest-frame drop | single slot; `unique_ptr` overwrite |
| [video_ring_buffer.cpp](./video_ring_buffer.cpp) | Pre-event rolling clip | FreeList + ring + snapshot refs |
| [local_sd_card_writer.cpp](./local_sd_card_writer.cpp) | Fan-out pipeline | `shared_ptr`; block vs drop-oldest |

Pool / allocator drills: [`../c++/`](../c++/) (`object_pool`, `two_level_mempool`).
SPSC/MPMC deep dives: [`../XR/24-无锁SPSC队列与Cacheline对齐.md`](../XR/24-无锁SPSC队列与Cacheline对齐.md),
[`../XR/25-无锁MPMC队列与CAS.md`](../XR/25-无锁MPMC队列与CAS.md).
Python mirrors (optional): `*_queue.py`, `*_ring_buffer.py`.

## Build & run

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure      # 15/15, ~1s
```

### Under ThreadSanitizer

Every concurrency change should be run under TSan. It is a happens-before race
detector: it flags a race even on a run that produced the correct answer.

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure   # 15/15 clean, ~21s

./build-tsan/data_race_demos --broken             # see real race reports
```

`-DENABLE_ASAN=ON` gives Address+UBSan instead (mutually exclusive with TSan).

Iteration counts auto-shrink 100x under sanitizers ([bench_scale.h](./bench_scale.h)) —
TSan needs one conflicting access pair, not a million.

## Notes on the build

- CMake strips `NDEBUG` from release configs. Every test asserts its invariants,
  and `NDEBUG` would compile them all away — turning the suite into a silent
  no-op. Verify your tests can actually fail.
- Timings printed by sanitizer builds are instrumentation noise; benchmark
  natively.

## Bugs found while building this (see STUDY_GUIDE.md §7)

Each of these was a genuine defect caught by the tests, and each is a good
interview story:

1. **Flush watermark used producer-assigned ids** — producers take an id from a
   shared atomic *then* append, so ids arrive out of order and events were
   silently lost. Fixed by having the ring assign its own sequence under its own
   lock.
2. **Query raced the flusher** — no watermark ordering gives both no-loss and
   no-duplicates. Chose the duplicate-producing order and dedupe explicitly:
   pick the failure mode you can repair.
3. **Drop counter cried wolf** — reported 802 drops while all 8000 events were
   safely stored, by counting overwrites of events the flusher had already
   copied out. Track "captured" separately from "durable".
4. **"Only one thread flushes" was a comment, not a guarantee** — `flush_now()`
   was public and could double-insert. Enforce single-writer invariants in code.
5. **`RelWithDebInfo` defines `NDEBUG`** — every `assert` vanished and the whole
   suite passed vacuously.
