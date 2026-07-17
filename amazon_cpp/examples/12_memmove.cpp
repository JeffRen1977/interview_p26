// Interview whiteboard: memmove with overlap handling + word copy.
//
// What Amazon L6 actually probes:
// 1. Overlap: memcpy assumes no overlap (UB if overlap); memmove must be safe.
// 2. Alignment: prefer 8-byte (uint64_t) copies on 64-bit CPUs when both ends allow it.
// 3. Follow-ups: unaligned access across ISAs, SIMD (AVX/NEON), non-temporal stores.
//
// This is a teaching implementation. glibc/musl use architecture-tuned assembly.

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

#if defined(__GNUC__) || defined(__clang__)
inline bool unlikely(bool expr) {
    return __builtin_expect(expr, 0);
}
#else
inline bool unlikely(bool expr) {
    return expr;
}
#endif

}  // namespace

// Safe move: correctly handles overlapping regions.
// Returns dest (same contract as libc memmove).
void* optimized_memmove(void* dest, const void* src, std::size_t count) {
    // Datapath hot path: null / zero-size are rare; hint the branch predictor.
    if (unlikely(dest == nullptr || src == nullptr || count == 0)) {
        return dest;
    }

    // Work in bytes so pointer arithmetic is well-defined.
    auto* d = static_cast<std::uint8_t*>(dest);
    const auto* s = static_cast<const std::uint8_t*>(src);

    // -------------------------------------------------------------------------
    // Overlap rule (the make-or-break interview check):
    //
    //   [==== src ====]
    //           [==== dest ====]   // dest starts inside src → FORWARD would
    //                              // overwrite unread src bytes. Copy BACKWARD.
    //
    //   [==== dest ====]
    //           [==== src ====]    // or no overlap → FORWARD is safe.
    //
    // dest == src is a no-op for the data; we still return dest.
    // -------------------------------------------------------------------------
    if (d > s && d < s + count) {
        // -------------------- Backward copy --------------------
        d += count;
        s += count;

        // Word path only when BOTH ending addresses are 8-byte aligned.
        // Otherwise casting to uint64_t* is C++ UB (and may fault on some ARM).
        if ((reinterpret_cast<std::uintptr_t>(d) & 7u) == 0 &&
            (reinterpret_cast<std::uintptr_t>(s) & 7u) == 0) {
            // Peel the unaligned-size tail (0..7 bytes) from the end first,
            // so the remaining region length is a multiple of 8 and both
            // pointers stay 8-byte aligned for the word loop.
            std::size_t tail = count & 7u;  // count % 8
            while (tail--) {
                *--d = *--s;
            }

            std::size_t blocks = count / 8;
            auto* d64 = reinterpret_cast<std::uint64_t*>(d);
            const auto* s64 = reinterpret_cast<const std::uint64_t*>(s);
            while (blocks--) {
                *--d64 = *--s64;
            }
            return dest;
        }

        // Misaligned / small: safe byte-by-byte backward copy.
        while (count--) {
            *--d = *--s;
        }
        return dest;
    }

    // -------------------- Forward copy --------------------
    // Same alignment gate: only widen to 8-byte loads/stores when both starts
    // are 8-byte aligned. If only one side is aligned, stay on bytes (or, in a
    // production version, peel dest up to alignment then copy words carefully).
    if ((reinterpret_cast<std::uintptr_t>(d) & 7u) == 0 &&
        (reinterpret_cast<std::uintptr_t>(s) & 7u) == 0) {
        std::size_t blocks = count / 8;
        std::size_t tail = count & 7u;

        auto* d64 = reinterpret_cast<std::uint64_t*>(d);
        const auto* s64 = reinterpret_cast<const std::uint64_t*>(s);
        while (blocks--) {
            *d64++ = *s64++;  // one bus transaction ≈ 8 bytes on 64-bit
        }

        d = reinterpret_cast<std::uint8_t*>(d64);
        s = reinterpret_cast<const std::uint8_t*>(s64);
        while (tail--) {
            *d++ = *s++;
        }
        return dest;
    }

    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

// ---------------------------------------------------------------------------
// Tests: overlap cases are the ones interviewers care about most.
// ---------------------------------------------------------------------------

static void expect_eq(const void* a, const void* b, std::size_t n,
                      const char* label) {
    assert(std::memcmp(a, b, n) == 0 && label);
}

void test_no_overlap_forward() {
    char src[] = "ABCDEFGH";
    char dst[16] = {};
    optimized_memmove(dst, src, 8);
    expect_eq(dst, "ABCDEFGH", 8, "no-overlap");
}

void test_overlap_backward() {
    // dest starts inside src → must copy backward.
    //
    // before:  [0 1 2 3 4 5 6 7]
    // move 6 bytes from index 0 → index 2
    // after:   [0 1 0 1 2 3 4 5]
    std::array<char, 8> buf = {'0', '1', '2', '3', '4', '5', '6', '7'};
    optimized_memmove(buf.data() + 2, buf.data(), 6);

    const char expect[] = {'0', '1', '0', '1', '2', '3', '4', '5'};
    expect_eq(buf.data(), expect, 8, "overlap-backward");
}

void test_overlap_forward() {
    // dest is before src → forward copy is required / safe.
    //
    // before:  [0 1 2 3 4 5 6 7]
    // move 6 bytes from index 2 → index 0
    // after:   [2 3 4 5 6 7 6 7]
    std::array<char, 8> buf = {'0', '1', '2', '3', '4', '5', '6', '7'};
    optimized_memmove(buf.data(), buf.data() + 2, 6);

    const char expect[] = {'2', '3', '4', '5', '6', '7', '6', '7'};
    expect_eq(buf.data(), expect, 8, "overlap-forward");
}

void test_word_aligned_bulk() {
    // Both pointers 8-byte aligned, length not multiple of 8 → word + tail.
    alignas(8) std::array<std::uint8_t, 24> src{};
    alignas(8) std::array<std::uint8_t, 24> dst{};
    for (std::size_t i = 0; i < src.size(); ++i) {
        src[i] = static_cast<std::uint8_t>(i + 1);
    }

    optimized_memmove(dst.data(), src.data(), 19);
    expect_eq(dst.data(), src.data(), 19, "aligned-bulk");
    assert(dst[19] == 0);  // untouched tail bytes stay zero
}

void test_matches_libc_memmove() {
    // Random-ish patterns vs libc as oracle (including overlap).
    std::vector<std::uint8_t> a(64);
    std::vector<std::uint8_t> b(64);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<std::uint8_t>(i * 3 + 7);
        b[i] = a[i];
    }

    optimized_memmove(a.data() + 5, a.data() + 1, 40);
    std::memmove(b.data() + 5, b.data() + 1, 40);
    expect_eq(a.data(), b.data(), a.size(), "vs-libc-overlap");

    optimized_memmove(a.data() + 1, a.data() + 8, 30);
    std::memmove(b.data() + 1, b.data() + 8, 30);
    expect_eq(a.data(), b.data(), a.size(), "vs-libc-forward-overlap");
}

int main() {
    test_no_overlap_forward();
    test_overlap_backward();
    test_overlap_forward();
    test_word_aligned_bulk();
    test_matches_libc_memmove();
    std::cout << "12_memmove: ok\n";
    return 0;
}
