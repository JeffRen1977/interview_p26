# Chapter 25. Coding for Physical AI Roles

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part VIII · target ~6,000 words · 3 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): c++/*; leetcode/*; company/amazon/examples/* -->

> C++ is required, not optional. Memory management, data structures, and patterns that Physical AI roles test for. 15 worked problems.

## Industry technology (2025–2026)

- **C++ standard:** C++20 is the baseline for Physical AI interviews (concepts, ranges, coroutines, `std::jthread`, `std::span`, `std::format`). C++23 features (`std::expected`, `std::mdspan`, `std::print`) are appearing.
- **Smart pointers:** `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`, `std::make_unique` — interviewers test ownership semantics, not just syntax
- **Physical AI-specific data structures:** Lock-free SPSC ring buffer, memory pool with fixed-block allocation, aligned allocators (for SIMD/NPU), bitset-based free lists, spatial hash maps (for robot perception), circular buffers for sensor streams
- **Patterns:** Producer-consumer (sensor → model), event dispatcher (multi-sensor fusion), state machines (robot control modes), RAII for hardware resources (DMA buffers, GPU contexts), zero-copy data sharing
- **Build & test:** CMake + Ninja, Google Test, Google Benchmark, AddressSanitizer / ThreadSanitizer, Valgrind, Clang-Tidy

## What the loops actually ask

15 worked problems with full C++ implementations:

1. **Lock-free SPSC ring buffer** — power-of-2 size, `std::atomic` with explicit memory ordering, cache-line alignment
2. **Fixed-block memory pool** — pre-allocated, O(1) alloc/free, no `new`/`delete` in the hot path
3. **Aligned malloc / free pair** — for SIMD and NPU buffer alignment (64-byte, 4096-byte)
4. **LRU cache** — `std::unordered_map` + doubly linked list, O(1) get/put (KV cache eviction)
5. **Thread-safe bounded blocking queue** — `std::mutex` + `std::condition_variable`, for multi-consumer pipelines
6. **Producer-consumer event dispatcher** — type-erased events, multiple listeners, thread-safe
7. **Hierarchical state machine** — for robot control modes (idle → moving → grasping → error)
8. **Trie for command parsing** — efficient prefix matching for voice commands
9. **Min-heap / priority queue from scratch** — for task scheduling in real-time systems
10. **Sliding-window maximum** — O(N) deque-based, for sensor signal processing
11. **Bit manipulation: popcount, leading zeros, power-of-2 check** — for memory allocators and bitmasks
12. **Matrix multiply (naive → tiled)** — understand cache effects, then profile with Google Benchmark
13. **Move semantics deep dive** — implement a non-copyable, movable buffer class with RAII
14. **`std::shared_ptr` from scratch** — reference counting, weak references, thread safety
15. **Serialize/deserialize a binary message** — for sensor data protocols (custom binary format, endianness)

## Companion code

- `ch25/memory_pool.h` — Fixed-block memory pool (C++20): O(1) alloc/free, no heap allocation in hot path
- `ch25/aligned_alloc.h` — Aligned malloc/free pair (C++17): 64-byte / 4096-byte alignment for SIMD/NPU DMA
- `ch25/lru_cache.h` — LRU cache (C++17): unordered_map + doubly linked list, O(1) get/put
- `ch25/spsc_ring_buffer.h` — Lock-free SPSC ring buffer (C++17, power-of-2, cache-line aligned)
- `ch25/state_machine.h` — Hierarchical state machine for robot control modes
- `ch25/move_semantics.h` — Non-copyable, movable RAII buffer class demonstrating move semantics

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
