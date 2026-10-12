# Chapter 5. Concurrency for Physical AI

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part II · target ~6,000 words · 5 figures · readiness: High — restructure existing chapter -->
<!-- Private source notes (write fresh; never paste): concurrency/CHAPTER_CONCURRENCY.md; concurrency/*.cpp -->

> Why physical AI is a concurrency problem: multiple sensors, multiple consumers, hard deadlines. Thread pools, lock-free queues, and bounded blocking queues. The latest-value pattern. Deadlock discipline. Real-time scheduling: priority inversion, priority inheritance, RT patches.

## Industry technology (2025–2026)

- **C++ concurrency:** C++20 `std::jthread`, `std::stop_token`, `std::latch`, `std::barrier`; C++23 `std::expected`; `std::atomic_ref` for non-owning atomic access
- **Lock-free libraries:** Folly `MPMCQueue` (Meta), Boost.Lockfree `spsc_queue`, `moodycamel::ConcurrentQueue`
- **RT scheduling:** Linux PREEMPT_RT (mainlined in kernel 6.x), `SCHED_FIFO`/`SCHED_DEADLINE`, `pthread_mutexattr_setprotocol(PTHREAD_PRIO_INHERIT)` for priority inheritance
- **Robot middleware:** ROS 2 Jazzy (2024) with real-time executor, Zenoh (zero-copy pub/sub, replacing DDS), micro-ROS (on MCUs)
- **Profiling:** Tracy profiler (frame-level), Linux `perf sched`, `ftrace`, Android Perfetto (system-wide tracing)

## What the loops actually ask

- Implement a lock-free SPSC ring buffer in C++. Explain the memory ordering of each atomic operation.
- When should you drop data instead of queuing it? Give a Physical AI example.
- Explain priority inversion. How does `PTHREAD_PRIO_INHERIT` fix it? What's the Mars Pathfinder story?
- Design a multi-sensor pipeline: camera at 30 fps, LiDAR at 10 Hz, IMU at 200 Hz, feeding a fusion model at 30 Hz. Draw the thread architecture.
- Your robot's control loop misses its 10 ms deadline 2% of the time. Diagnose it using `perf sched` or Perfetto traces.
- Compare `std::mutex` + `std::condition_variable` vs a lock-free queue for a producer-consumer pipeline. When does each win?

## Companion code

- `ch05/blocking_queue.h` — Bounded blocking queue (C++20, mutex + condition_variable, multi-consumer safe)
- `ch05/pi_mutex.cpp` — Priority inheritance mutex setup (POSIX `PTHREAD_PRIO_INHERIT`)
- `ch05/spsc_queue.h` — Lock-free SPSC queue (C++17, from Ch 4, with tests)
- `ch05/multi_sensor_pipeline.cpp` — Multi-sensor pipeline example: 3 producers, 2 consumers, RT scheduling

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises
