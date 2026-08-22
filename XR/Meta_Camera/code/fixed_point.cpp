// Fixed-point arithmetic without an FPU: Q-format multiply/divide, integer
// sqrt, reciprocal, gamma LUT, and rounding that does not drift.
//
// Why this shows up in a camera interview: 3A runs on a DSP or an M-class core
// with no hardware float, digital gain and CCM are applied per pixel, and a
// gamma curve applied with truncation instead of rounding visibly bands the
// shadows. "Do it in integers, and prove you did not lose a bit" is the round.

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Q-format. Qm.n means n fractional bits: value = raw / 2^n.
//    Q16.16 in an int32_t covers [-32768, 32768) with 1/65536 resolution.
// ---------------------------------------------------------------------------

using q16 = int32_t;
static const int Q = 16;
static const int32_t Q_ONE = 1 << Q;

static inline q16 to_q16(double v) { return static_cast<q16>(std::lround(v * Q_ONE)); }
static inline double from_q16(q16 v) { return static_cast<double>(v) / Q_ONE; }

// Multiply: the product needs 2n fractional bits, so widen FIRST. Doing
// (a * b) >> Q in int32_t overflows for anything above ~1.0 — this is the
// single most common bug in the answer.
static inline q16 qmul(q16 a, q16 b) {
    const int64_t p = static_cast<int64_t>(a) * b;
    // Round-half-away-from-zero instead of truncating toward -inf.
    const int64_t half = static_cast<int64_t>(1) << (Q - 1);
    return static_cast<q16>((p >= 0 ? (p + half) : (p - half)) >> Q);
}

static inline q16 qdiv(q16 a, q16 b) {
    assert(b != 0);
    const int64_t num = static_cast<int64_t>(a) << Q;
    // Round the quotient rather than truncating.
    const int64_t h = std::abs(static_cast<int64_t>(b)) / 2;
    return static_cast<q16>(((num >= 0) ? (num + h) : (num - h)) / b);
}

// ---------------------------------------------------------------------------
// 2. Integer sqrt, exact floor. Binary "digit by digit" method: no division,
//    no float, 16 iterations for a 32-bit input. Used for gradient magnitude
//    and for the distance term in lens-shading correction.
// ---------------------------------------------------------------------------

uint32_t isqrt32(uint32_t n) {
    uint32_t rem = 0, root = 0;
    for (int i = 0; i < 16; ++i) {
        root <<= 1;
        rem = (rem << 2) | (n >> 30);
        n <<= 2;
        if (rem > root) {
            rem -= root | 1;
            root |= 2;
        }
    }
    return root >> 1;
}

// Same algorithm widened to 64 bits.
uint64_t isqrt64(uint64_t n) {
    uint64_t rem = 0, root = 0;
    for (int i = 0; i < 32; ++i) {
        root <<= 1;
        rem = (rem << 2) | (n >> 62);
        n <<= 2;
        if (rem > root) {
            rem -= root | 1;
            root |= 2;
        }
    }
    return root >> 1;
}

// sqrt in Q16.16: sqrt(raw / 2^16) expressed in Q16.16 == isqrt(raw * 2^16).
// `raw << 16` needs 64 bits — doing it in uint32_t silently truncates every
// value above 1.0, which is exactly the bug this drill exists to teach.
q16 qsqrt(q16 x) {
    assert(x >= 0);
    return static_cast<q16>(isqrt64(static_cast<uint64_t>(x) << Q));
}

// ---------------------------------------------------------------------------
// 3. Reciprocal by Newton-Raphson, for the case where the divider is slow or
//    absent. x_{k+1} = x_k * (2 - d * x_k). Converges quadratically once the
//    initial guess is within range, so normalise d into [0.5, 1) first.
// ---------------------------------------------------------------------------

q16 qrecip(q16 d) {
    assert(d > 0);
    // Normalise: find s such that d << s lands in [0.5, 1) in Q16.16.
    int s = 0;
    q16 dn = d;
    while (dn < (Q_ONE >> 1)) { dn <<= 1; ++s; }
    while (dn >= Q_ONE) { dn >>= 1; --s; }

    // Seed: 48/17 - 32/17 * dn, the standard minimax start on [0.5, 1).
    q16 x = to_q16(48.0 / 17.0) - qmul(to_q16(32.0 / 17.0), dn);
    for (int i = 0; i < 4; ++i)
        x = qmul(x, (2 * Q_ONE) - qmul(dn, x));

    // Undo the normalisation: 1/d = (1/dn) << s
    return (s >= 0) ? static_cast<q16>(x << s) : static_cast<q16>(x >> (-s));
}

// ---------------------------------------------------------------------------
// 4. Gamma as a LUT. Never evaluate pow() per pixel; build 256 entries once.
//    Rounding matters: truncation loses roughly half an LSB everywhere and
//    banding shows up in the shadows where the curve is steepest.
// ---------------------------------------------------------------------------

void build_gamma_lut(double gamma, uint8_t lut[256]) {
    for (int i = 0; i < 256; ++i) {
        const double v = std::pow(i / 255.0, 1.0 / gamma) * 255.0;
        const int r = static_cast<int>(v + 0.5);            // round, not truncate
        lut[i] = static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
    }
}

// ---------------------------------------------------------------------------
// 5. Digital gain + black-level in fixed point, the actual per-pixel operation.
//    out = clamp((in - black) * gain), gain in Q8.8 so a 4x gain is 1024.
// ---------------------------------------------------------------------------

void apply_gain_blc(const uint16_t* src, uint16_t* dst, int n,
                    uint16_t black_level, uint16_t gain_q8_8, uint16_t white_level) {
    for (int i = 0; i < n; ++i) {
        int32_t v = static_cast<int32_t>(src[i]) - black_level;
        if (v < 0) v = 0;                                    // clip below black, do not wrap
        // +128 rounds the >>8 instead of truncating.
        int32_t out = (v * gain_q8_8 + 128) >> 8;
        if (out > white_level) out = white_level;
        dst[i] = static_cast<uint16_t>(out);
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_qmul_no_overflow() {
    // 200.0 * 150.0 = 30000.0 — int32 intermediate would have wrapped long ago.
    const q16 a = to_q16(200.0), b = to_q16(150.0);
    assert(std::abs(from_q16(qmul(a, b)) - 30000.0) < 0.01);
    // Signs.
    assert(std::abs(from_q16(qmul(to_q16(-2.5), to_q16(4.0))) + 10.0) < 1e-4);
    assert(std::abs(from_q16(qmul(to_q16(-2.5), to_q16(-4.0))) - 10.0) < 1e-4);
    // Identity.
    assert(qmul(to_q16(7.25), Q_ONE) == to_q16(7.25));
}

static void test_qmul_rounds_symmetrically() {
    // Truncation biases negatives downward; the +/- half must fix it.
    const q16 tiny = 3;                       // 3 / 65536
    assert(qmul(tiny, Q_ONE / 2) == 2);       // 1.5 -> 2
    assert(qmul(-tiny, Q_ONE / 2) == -2);     // -1.5 -> -2, not -1
}

static void test_qdiv() {
    assert(std::abs(from_q16(qdiv(to_q16(1.0), to_q16(3.0))) - 1.0 / 3.0) < 1e-4);
    assert(std::abs(from_q16(qdiv(to_q16(-7.0), to_q16(2.0))) + 3.5) < 1e-4);
    assert(std::abs(from_q16(qdiv(to_q16(7.0), to_q16(-2.0))) + 3.5) < 1e-4);
}

static void test_isqrt_exact() {
    for (uint32_t n : {0u, 1u, 2u, 3u, 4u, 8u, 9u, 15u, 16u, 1023u, 1024u, 65535u,
                       65536u, 1000000u, 0xFFFFFFFEu, 0xFFFFFFFFu}) {
        const uint32_t r = isqrt32(n);
        assert(static_cast<uint64_t>(r) * r <= n);
        assert(static_cast<uint64_t>(r + 1) * (r + 1) > n);
    }
    // Exhaustive on a small range, plus every perfect square boundary.
    for (uint32_t n = 0; n < 100000; ++n) {
        const uint32_t r = isqrt32(n);
        assert(static_cast<uint64_t>(r) * r <= n && static_cast<uint64_t>(r + 1) * (r + 1) > n);
    }
    for (uint32_t k = 1; k < 65535; k += 977) {
        assert(isqrt32(k * k) == k);
        assert(isqrt32(k * k - 1) == k - 1);
    }
}

static void test_qsqrt() {
    for (double v : {0.25, 1.0, 2.0, 100.0, 1234.5}) {
        const double got = from_q16(qsqrt(to_q16(v)));
        assert(std::abs(got - std::sqrt(v)) < 1e-3);
    }
}

static void test_qrecip() {
    for (double d : {0.1, 0.5, 0.75, 1.0, 2.0, 3.0, 10.0, 100.0}) {
        const double got = from_q16(qrecip(to_q16(d)));
        assert(std::abs(got - 1.0 / d) < 1e-3 * (1.0 / d) + 1e-4);
    }
}

static void test_gamma_lut_endpoints_and_monotonic() {
    uint8_t lut[256];
    build_gamma_lut(2.2, lut);
    assert(lut[0] == 0 && lut[255] == 255);
    for (int i = 1; i < 256; ++i) assert(lut[i] >= lut[i - 1]);
    // Gamma 2.2 lifts mid-grey.
    assert(lut[128] > 128);
    // Gamma 1.0 must be identity — the test that catches an off-by-one in the
    // normalisation (dividing by 256 instead of 255).
    build_gamma_lut(1.0, lut);
    for (int i = 0; i < 256; ++i) assert(lut[i] == i);
}

static void test_gain_blc() {
    const uint16_t src[] = {0, 64, 100, 500, 1023};
    uint16_t dst[5];
    // black = 64, gain = 2.0 (Q8.8 = 512), white = 1023
    apply_gain_blc(src, dst, 5, 64, 512, 1023);
    assert(dst[0] == 0);           // below black, clipped not wrapped
    assert(dst[1] == 0);
    assert(dst[2] == 72);          // (100-64)*2
    assert(dst[3] == 872);         // (500-64)*2
    assert(dst[4] == 1023);        // (1023-64)*2 = 1918, clamped to white
}

static void test_gain_rounding() {
    // gain = 1.5 (384 in Q8.8) on value 1 -> 1.5 -> rounds to 2, not 1.
    const uint16_t src[] = {1};
    uint16_t dst[1];
    apply_gain_blc(src, dst, 1, 0, 384, 4095);
    assert(dst[0] == 2);
}

int main() {
    test_qmul_no_overflow();
    test_qmul_rounds_symmetrically();
    test_qdiv();
    test_isqrt_exact();
    test_qsqrt();
    test_qrecip();
    test_gamma_lut_endpoints_and_monotonic();
    test_gain_blc();
    test_gain_rounding();
    std::cout << "fixed_point: ok\n";
    return 0;
}
