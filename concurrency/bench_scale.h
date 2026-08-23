// Iteration-count scaling for sanitizer builds.
//
// ThreadSanitizer instruments every memory access and is roughly 5-15x slower
// (far worse for tight atomic loops, where it can be 100x). A benchmark tuned
// to run in 200ms natively will sit there for minutes under TSan.
//
// So: detect the sanitizer at compile time and shrink the loop counts. The
// tests still exercise the same code paths and still find races -- TSan detects
// a race from a single conflicting pair of accesses, it does not need volume.
// Only the benchmark *numbers* are meaningless under instrumentation, and you
// should never trust those anyway.
//
// Worth knowing for the interview: "I run the stress tests under TSan with
// reduced iteration counts, because TSan needs one conflicting access pair,
// not a million" is a concrete, credible answer to "how do you test this?"

#ifndef BENCH_SCALE_H_
#define BENCH_SCALE_H_

#include <cstddef>

#if defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
#define CONCURRENCY_SANITIZER_BUILD 1
#endif
#endif
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
#define CONCURRENCY_SANITIZER_BUILD 1
#endif

#if defined(CONCURRENCY_SANITIZER_BUILD)
constexpr int kBenchScale = 100;  // divide iteration counts by this
constexpr bool kSanitizerBuild = true;
#else
constexpr int kBenchScale = 1;
constexpr bool kSanitizerBuild = false;
#endif

// Scale an iteration count, never below `floor` so the test still does work.
constexpr int scaled(int iterations, int floor = 100) {
    const int n = iterations / kBenchScale;
    return n < floor ? floor : n;
}

#endif  // BENCH_SCALE_H_
