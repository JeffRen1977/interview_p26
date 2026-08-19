// Circular bit rotation (ROTL / ROTR) — Meta Camera coding drill.
//
// ROTL_N(x, k) = (x << k) | (x >> (N - k))
// Must mask k with (N - 1): a shift of N bits is undefined in C/C++.
// When k == 0, N - k == N, so (x >> N) is UB unless we use ((-k) & mask).

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

constexpr uint32_t rotl32(uint32_t x, unsigned int k) {
    const unsigned int mask = 31;
    k &= mask;
    return (x << k) | (x >> ((-k) & mask));
}

constexpr uint32_t rotr32(uint32_t x, unsigned int k) {
    const unsigned int mask = 31;
    k &= mask;
    return (x >> k) | (x << ((-k) & mask));
}

constexpr uint64_t rotl64(uint64_t x, unsigned int k) {
    const unsigned int mask = 63;
    k &= mask;
    return (x << k) | (x >> ((-k) & mask));
}

void rotl32_buffer(const uint32_t* src, uint32_t* dst, size_t n, unsigned int k) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = rotl32(src[i], k);
    }
}

#if defined(__ARM_NEON)
#include <arm_neon.h>

inline uint32x4_t rotl32_neon(uint32x4_t val, int shift) {
    shift &= 31;
    uint32x4_t left = vshlq_u32(val, vdupq_n_s32(shift));
    uint32x4_t right = vshlq_u32(val, vdupq_n_s32(-(32 - shift)));
    return vorrq_u32(left, right);
}
#endif

static void test_known_values() {
    assert(rotl32(0x12345678u, 4) == 0x23456781u);
    assert(rotr32(0x12345678u, 4) == 0x81234567u);
    assert(rotl32(1u, 1) == 2u);
    assert(rotl32(0x80000000u, 1) == 1u);
    assert(rotr32(1u, 1) == 0x80000000u);
}

static void test_k_zero_and_multiple_of_width() {
    const uint32_t x = 0xA5A5A5A5u;
    assert(rotl32(x, 0) == x);
    assert(rotr32(x, 0) == x);
    assert(rotl32(x, 32) == x);
    assert(rotr32(x, 32) == x);
    assert(rotl32(x, 64) == x);
    assert(rotl32(x, 33) == rotl32(x, 1));
}

static void test_inverse_and_dual() {
    const uint32_t x = 0xDEADBEEFu;
    for (unsigned k = 0; k < 64; ++k) {
        assert(rotr32(rotl32(x, k), k) == x);
        assert(rotl32(rotr32(x, k), k) == x);
        assert(rotl32(x, k) == rotr32(x, 32u - (k & 31u)));
    }
}

static void test_rotl64() {
    const uint64_t x = 0x0123456789ABCDEFULL;
    assert(rotl64(x, 0) == x);
    assert(rotl64(x, 64) == x);
    assert(rotl64(x, 4) == 0x123456789ABCDEF0ULL);
    assert(rotl64(rotl64(x, 17), 47) == x);
}

static void test_buffer() {
    const uint32_t src[] = {0x12345678u, 0x80000001u, 0u, 0xFFFFFFFFu};
    uint32_t dst[4] = {};
    rotl32_buffer(src, dst, 4, 4);
    assert(dst[0] == rotl32(src[0], 4));
    assert(dst[1] == rotl32(src[1], 4));
    assert(dst[2] == 0);
    assert(dst[3] == 0xFFFFFFFFu);
}

#if defined(__ARM_NEON)
static void test_neon_matches_scalar() {
    uint32_t lanes[4] = {0x12345678u, 0x80000001u, 0u, 0xFFFFFFFFu};
    for (int shift = 0; shift < 32; ++shift) {
        uint32x4_t v = vld1q_u32(lanes);
        uint32_t out[4];
        vst1q_u32(out, rotl32_neon(v, shift));
        for (int i = 0; i < 4; ++i) {
            assert(out[i] == rotl32(lanes[i], static_cast<unsigned>(shift)));
        }
    }
}
#endif

int main() {
    test_known_values();
    test_k_zero_and_multiple_of_width();
    test_inverse_and_dual();
    test_rotl64();
    test_buffer();
#if defined(__ARM_NEON)
    test_neon_matches_scalar();
#endif
    std::cout << "bit_rotation: ok\n";
    return 0;
}
