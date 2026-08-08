# Concurrency interview sketches

Camera / systems-style concurrency problems with clear problem statements and
runnable C++ demos.

## Problems

| File | Problem | Key idea |
|------|---------|----------|
| [bounded_blocking_queue.cpp](./bounded_blocking_queue.cpp) | Bounded blocking queue | mutex + 2 CVs; put/get block |
| [thread_safe_queue.cpp](./thread_safe_queue.cpp) | Minimal bounded queue | whiteboard skeleton of the above |
| [thread_safe_ring_buffer.cpp](./thread_safe_ring_buffer.cpp) | MPMC ring (mutex) | circular buffer; non-blocking push/pop |
| [spsc_buffer.cpp](./spsc_buffer.cpp) | Lock-free SPSC (minimal) | sentinel slot; acquire/release |
| [spsc_ring_buffer.cpp](./spsc_ring_buffer.cpp) | Lock-free SPSC (full) | cache-line padding; tests |
| [producer_consumer_frame_dropping.cpp](./producer_consumer_frame_dropping.cpp) | Latest-frame drop | single slot; `unique_ptr` overwrite |
| [video_ring_buffer.cpp](./video_ring_buffer.cpp) | Pre-event rolling clip | FreeList + ring + snapshot refs |
| [local_sd_card_writer.cpp](./local_sd_card_writer.cpp) | Fan-out pipeline | shared_ptr; block vs drop-oldest |
| [object_pool.cpp](./object_pool.cpp) | Object pool | prealloc; acquire/release |
| [two_level_mempool.cpp](./two_level_mempool.cpp) | Two-level mempool | Treiber list + local cache; ABA |

Python mirrors (optional): `*_queue.py`, `*_ring_buffer.py`, `object_pool.py`.

## Build & run

```bash
cd concurrency
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Review notes (what was fixed)

1. **Frame dropping** — must not reuse the same `Frame` from a shared pool while
   the consumer still holds it; use exclusive `unique_ptr` ownership.
2. **Video ring snapshot** — iterate oldest→newest from `head_`, not raw `[0..N)`.
3. **SD fan-out queue** — `pop` must `notify` not-full waiters; prefer two CVs.
4. **SPSC sketch** — `is_full` must match sentinel-slot policy (`size-1` usable).
