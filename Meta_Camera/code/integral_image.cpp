// Integral image (summed-area table) + O(1) box filter of any radius.
// The natural follow-up to box_filter.cpp: "now make the 15x15 blur not cost
// 225 loads per pixel."

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Width of the accumulator is the whole question.
//   8-bit pixels, 4096 x 4096 image -> 255 * 2^24 = 2^32 - 2^24, which just
//   fits uint32_t. One more doubling of resolution, or 10-bit input, and it
//   does not. Use uint64_t unless you can prove the bound; it costs bandwidth,
//   not correctness. State this before writing the loop.
// ---------------------------------------------------------------------------
using Sum = uint64_t;

// sat has (height+1) x (width+1) entries: row 0 and column 0 are zero, so the
// rectangle query needs no branches at the image border.
void build_integral(const uint8_t* src, int width, int height, int src_stride,
                    Sum* sat, int sat_stride) {
    for (int c = 0; c <= width; ++c) sat[c] = 0;
    for (int r = 0; r < height; ++r) {
        const uint8_t* srow = src + static_cast<size_t>(r) * src_stride;
        Sum* prev = sat + static_cast<size_t>(r) * sat_stride;
        Sum* cur = sat + static_cast<size_t>(r + 1) * sat_stride;
        cur[0] = 0;
        Sum row_acc = 0;
        for (int c = 0; c < width; ++c) {
            row_acc += srow[c];
            cur[c + 1] = prev[c + 1] + row_acc;
        }
    }
}

// Inclusive rectangle [y0, y1] x [x0, x1] in image coordinates.
static inline Sum rect_sum(const Sum* sat, int sat_stride, int x0, int y0, int x1, int y1) {
    const Sum A = sat[static_cast<size_t>(y0) * sat_stride + x0];
    const Sum B = sat[static_cast<size_t>(y0) * sat_stride + (x1 + 1)];
    const Sum C = sat[static_cast<size_t>(y1 + 1) * sat_stride + x0];
    const Sum D = sat[static_cast<size_t>(y1 + 1) * sat_stride + (x1 + 1)];
    return D - B - C + A;
}

// Box filter of radius `rad` ((2*rad+1)^2 window), replicate padding, O(1)/px.
//
// Replicate padding via a SAT needs care: you cannot just clamp the query
// rectangle, because that shrinks the window and changes the divisor. Clamping
// the rectangle *and* dividing by the clamped area gives you "average of the
// available pixels", which is what clamped-coordinate replicate padding
// produces only in the interior. Here we clamp both, and match box_filter.cpp
// on the interior — the border differs, and you should say so.
void box_filter_sat(const uint8_t* src, int width, int height, int src_stride,
                    int rad, uint8_t* dst, int dst_stride) {
    const int ss = width + 1;
    std::vector<Sum> sat(static_cast<size_t>(ss) * (height + 1));
    build_integral(src, width, height, src_stride, sat.data(), ss);

    for (int r = 0; r < height; ++r) {
        uint8_t* drow = dst + static_cast<size_t>(r) * dst_stride;
        const int y0 = (r - rad < 0) ? 0 : r - rad;
        const int y1 = (r + rad >= height) ? height - 1 : r + rad;
        for (int c = 0; c < width; ++c) {
            const int x0 = (c - rad < 0) ? 0 : c - rad;
            const int x1 = (c + rad >= width) ? width - 1 : c + rad;
            const Sum s = rect_sum(sat.data(), ss, x0, y0, x1, y1);
            const Sum area = static_cast<Sum>(x1 - x0 + 1) * static_cast<Sum>(y1 - y0 + 1);
            drow[c] = static_cast<uint8_t>(s / area);
        }
    }
}

// Naive reference, same clamped-window semantics.
void box_filter_naive(const uint8_t* src, int width, int height, int src_stride,
                      int rad, uint8_t* dst, int dst_stride) {
    for (int r = 0; r < height; ++r) {
        uint8_t* drow = dst + static_cast<size_t>(r) * dst_stride;
        for (int c = 0; c < width; ++c) {
            Sum s = 0, n = 0;
            for (int dy = -rad; dy <= rad; ++dy) {
                const int y = r + dy;
                if (y < 0 || y >= height) continue;
                const uint8_t* srow = src + static_cast<size_t>(y) * src_stride;
                for (int dx = -rad; dx <= rad; ++dx) {
                    const int x = c + dx;
                    if (x < 0 || x >= width) continue;
                    s += srow[x]; ++n;
                }
            }
            drow[c] = static_cast<uint8_t>(s / n);
        }
    }
}

// ---------------------------------------------------------------------------
// What this is actually for in a camera: AE zone metering. The 3A block wants
// the mean of an NxM grid of zones every frame; one SAT answers all of them.
// ---------------------------------------------------------------------------
void ae_zone_means(const uint8_t* src, int width, int height, int src_stride,
                   int zones_x, int zones_y, std::vector<uint8_t>* out) {
    const int ss = width + 1;
    std::vector<Sum> sat(static_cast<size_t>(ss) * (height + 1));
    build_integral(src, width, height, src_stride, sat.data(), ss);

    out->assign(static_cast<size_t>(zones_x) * zones_y, 0);
    for (int zy = 0; zy < zones_y; ++zy) {
        const int y0 = zy * height / zones_y;
        const int y1 = (zy + 1) * height / zones_y - 1;
        for (int zx = 0; zx < zones_x; ++zx) {
            const int x0 = zx * width / zones_x;
            const int x1 = (zx + 1) * width / zones_x - 1;
            const Sum s = rect_sum(sat.data(), ss, x0, y0, x1, y1);
            const Sum area = static_cast<Sum>(x1 - x0 + 1) * static_cast<Sum>(y1 - y0 + 1);
            (*out)[static_cast<size_t>(zy) * zones_x + zx] = static_cast<uint8_t>(s / area);
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_integral_known() {
    // 3x3 of all 1s -> sat[r][c] = r * c
    const int w = 3, h = 3, stride = 5;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 0);
    for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) src[r * stride + c] = 1;
    const int ss = w + 1;
    std::vector<Sum> sat(static_cast<size_t>(ss) * (h + 1));
    build_integral(src.data(), w, h, stride, sat.data(), ss);
    for (int r = 0; r <= h; ++r)
        for (int c = 0; c <= w; ++c)
            assert(sat[static_cast<size_t>(r) * ss + c] == static_cast<Sum>(r) * c);
}

static void test_rect_sum_against_bruteforce() {
    const int w = 9, h = 7, stride = 11;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 0);
    uint32_t s = 7;
    auto rnd = [&s] { s = s * 1103515245u + 12345u; return static_cast<uint8_t>(s >> 24); };
    for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) src[r * stride + c] = rnd();

    const int ss = w + 1;
    std::vector<Sum> sat(static_cast<size_t>(ss) * (h + 1));
    build_integral(src.data(), w, h, stride, sat.data(), ss);

    for (int y0 = 0; y0 < h; ++y0)
      for (int y1 = y0; y1 < h; ++y1)
        for (int x0 = 0; x0 < w; ++x0)
          for (int x1 = x0; x1 < w; ++x1) {
            Sum ref = 0;
            for (int r = y0; r <= y1; ++r)
                for (int c = x0; c <= x1; ++c) ref += src[r * stride + c];
            assert(rect_sum(sat.data(), ss, x0, y0, x1, y1) == ref);
          }
}

static void test_sat_box_matches_naive() {
    const int w = 17, h = 13, stride = 24, ds = 20;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 0);
    uint32_t s = 99;
    auto rnd = [&s] { s = s * 1664525u + 1013904223u; return static_cast<uint8_t>(s >> 24); };
    for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) src[r * stride + c] = rnd();

    for (int rad : {1, 2, 3, 7}) {
        std::vector<uint8_t> a(static_cast<size_t>(ds) * h, 0), b = a;
        box_filter_sat(src.data(), w, h, stride, rad, a.data(), ds);
        box_filter_naive(src.data(), w, h, stride, rad, b.data(), ds);
        assert(a == b);
    }
}

static void test_flat_field_is_preserved() {
    const int w = 20, h = 20, stride = 20;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 137);
    std::vector<uint8_t> dst(static_cast<size_t>(stride) * h, 0);
    box_filter_sat(src.data(), w, h, stride, 5, dst.data(), stride);
    for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) assert(dst[r * stride + c] == 137);
}

static void test_ae_zones() {
    // Left half 200, right half 40; a 2x1 zone grid must read exactly that.
    const int w = 16, h = 8, stride = 16;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 0);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) src[r * stride + c] = (c < w / 2) ? 200 : 40;
    std::vector<uint8_t> zones;
    ae_zone_means(src.data(), w, h, stride, 2, 1, &zones);
    assert(zones.size() == 2 && zones[0] == 200 && zones[1] == 40);

    // 4x4 grid over the same frame: left two columns bright, right two dark.
    ae_zone_means(src.data(), w, h, stride, 4, 4, &zones);
    for (int zy = 0; zy < 4; ++zy) {
        assert(zones[zy * 4 + 0] == 200 && zones[zy * 4 + 1] == 200);
        assert(zones[zy * 4 + 2] == 40 && zones[zy * 4 + 3] == 40);
    }
}

int main() {
    test_integral_known();
    test_rect_sum_against_bruteforce();
    test_sat_box_matches_naive();
    test_flat_field_is_preserved();
    test_ae_zones();
    std::cout << "integral_image: ok\n";
    return 0;
}
