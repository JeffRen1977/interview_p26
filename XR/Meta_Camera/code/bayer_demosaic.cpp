// Bayer -> RGB bilinear demosaic (RGGB/BGGR/GRBG/GBRG) with stride.
// Meta Camera coding drill. The whole trick is: which colour is this pixel?

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

// CFA pattern named by the top-left 2x2 quad:
//   RGGB      GRBG      GBRG      BGGR
//   R G       G R       G B       B G
//   G B       B G       R G       G R
enum class Bayer { RGGB, GRBG, GBRG, BGGR };

// Colour of pixel (r, c): 0 = R, 1 = G, 2 = B.
static inline int cfa_color(Bayer p, int r, int c) {
    const int q = ((r & 1) << 1) | (c & 1);   // 0=TL 1=TR 2=BL 3=BR
    static const int tbl[4][4] = {
        /* RGGB */ {0, 1, 1, 2},
        /* GRBG */ {1, 0, 2, 1},
        /* GBRG */ {1, 2, 0, 1},
        /* BGGR */ {2, 1, 1, 0},
    };
    return tbl[static_cast<int>(p)][q];
}

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Mirror-reflect the coordinate so edge pixels keep the CFA phase.
// Reflecting by 2 (not 1) is what preserves parity — reflecting by 1 flips
// the colour of the neighbour and tints the border.
static inline int reflect2(int v, int n) {
    if (v < 0) return -v;               // -1 -> 1, -2 -> 2  (parity preserved)
    if (v >= n) return 2 * n - 2 - v;   // n -> n-2
    return v;
}

// Fetch the raw sample at (r, c) with parity-preserving reflection.
static inline int at(const uint16_t* src, int w, int h, int stride, int r, int c) {
    r = clampi(reflect2(r, h), 0, h - 1);
    c = clampi(reflect2(c, w), 0, w - 1);
    return src[static_cast<size_t>(r) * stride + c];
}

// Bilinear demosaic. dst is interleaved RGB, dst_stride in *samples* (>= 3*w).
void demosaic_bilinear(const uint16_t* src, int width, int height, int src_stride,
                       Bayer pattern, uint16_t* dst, int dst_stride) {
    for (int r = 0; r < height; ++r) {
        uint16_t* out = dst + static_cast<size_t>(r) * dst_stride;
        for (int c = 0; c < width; ++c) {
            const int color = cfa_color(pattern, r, c);
            const int self = at(src, width, height, src_stride, r, c);

            // 4-neighbour cross and 4 diagonals.
            const int N = at(src, width, height, src_stride, r - 1, c);
            const int S = at(src, width, height, src_stride, r + 1, c);
            const int W = at(src, width, height, src_stride, r, c - 1);
            const int E = at(src, width, height, src_stride, r, c + 1);
            const int NW = at(src, width, height, src_stride, r - 1, c - 1);
            const int NE = at(src, width, height, src_stride, r - 1, c + 1);
            const int SW = at(src, width, height, src_stride, r + 1, c - 1);
            const int SE = at(src, width, height, src_stride, r + 1, c + 1);

            int R, G, B;
            if (color == 1) {
                // Green site. Missing R and B sit on the two axes; which axis
                // carries R depends on the row's colour.
                G = self;
                const int horiz = (W + E + 1) / 2;
                const int vert  = (N + S + 1) / 2;
                // Colour of the left/right neighbour tells us the axis.
                if (cfa_color(pattern, r, c - 1) == 0) { R = horiz; B = vert; }
                else                                   { B = horiz; R = vert; }
            } else {
                // Red or Blue site. G = cross average, opposite = diagonal average.
                G = (N + S + W + E + 2) / 4;
                const int diag = (NW + NE + SW + SE + 2) / 4;
                if (color == 0) { R = self; B = diag; }
                else            { B = self; R = diag; }
            }
            out[c * 3 + 0] = static_cast<uint16_t>(R);
            out[c * 3 + 1] = static_cast<uint16_t>(G);
            out[c * 3 + 2] = static_cast<uint16_t>(B);
        }
    }
}

// ---------------------------------------------------------------------------
// Cheap alternative asked as a follow-up: 2x2 binning ("half-res demosaic").
// One output pixel per CFA quad — no interpolation, no false colour, half the
// resolution. This is what a tracking / thumbnail path actually runs.
// ---------------------------------------------------------------------------
void demosaic_binned_2x2(const uint16_t* src, int width, int height, int src_stride,
                         Bayer pattern, uint16_t* dst, int dst_stride) {
    const int ow = width / 2, oh = height / 2;
    for (int r = 0; r < oh; ++r) {
        uint16_t* out = dst + static_cast<size_t>(r) * dst_stride;
        for (int c = 0; c < ow; ++c) {
            int acc[3] = {0, 0, 0}, cnt[3] = {0, 0, 0};
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int sr = 2 * r + dy, sc = 2 * c + dx;
                    const int col = cfa_color(pattern, sr, sc);
                    acc[col] += src[static_cast<size_t>(sr) * src_stride + sc];
                    ++cnt[col];
                }
            for (int k = 0; k < 3; ++k)
                out[c * 3 + k] = static_cast<uint16_t>(acc[k] / cnt[k]);
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// A flat scene where every R sample = 100, every G = 200, every B = 300 must
// demosaic to exactly (100, 200, 300) everywhere, borders included.
// This is the test that catches a broken edge policy.
static void test_flat_field_all_patterns() {
    const int w = 8, h = 8, stride = 12;
    for (Bayer p : {Bayer::RGGB, Bayer::GRBG, Bayer::GBRG, Bayer::BGGR}) {
        std::vector<uint16_t> src(static_cast<size_t>(stride) * h, 0);
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const int col = cfa_color(p, r, c);
                src[static_cast<size_t>(r) * stride + c] =
                    static_cast<uint16_t>(col == 0 ? 100 : (col == 1 ? 200 : 300));
            }
        const int ds = w * 3 + 5;
        std::vector<uint16_t> dst(static_cast<size_t>(ds) * h, 0);
        demosaic_bilinear(src.data(), w, h, stride, p, dst.data(), ds);
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const uint16_t* px = &dst[static_cast<size_t>(r) * ds + c * 3];
                assert(px[0] == 100 && px[1] == 200 && px[2] == 300);
            }
    }
}

static void test_cfa_color_table() {
    assert(cfa_color(Bayer::RGGB, 0, 0) == 0);
    assert(cfa_color(Bayer::RGGB, 1, 1) == 2);
    assert(cfa_color(Bayer::BGGR, 0, 0) == 2);
    assert(cfa_color(Bayer::GRBG, 0, 1) == 0);
    assert(cfa_color(Bayer::GBRG, 1, 0) == 0);
}

static void test_reflect_preserves_parity() {
    // Reflected coordinate must land on the same CFA parity as the real one.
    for (int n : {8, 9}) {
        for (int v = -2; v < n + 2; ++v) {
            const int rv = clampi(reflect2(v, n), 0, n - 1);
            if (v >= 0 && v < n) assert(rv == v);
            else assert(((rv ^ v) & 1) == 0);
        }
    }
}

static void test_binning_flat() {
    const int w = 8, h = 8, stride = 8;
    std::vector<uint16_t> src(static_cast<size_t>(stride) * h, 0);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            const int col = cfa_color(Bayer::RGGB, r, c);
            src[static_cast<size_t>(r) * stride + c] =
                static_cast<uint16_t>(col == 0 ? 100 : (col == 1 ? 200 : 300));
        }
    const int ds = (w / 2) * 3;
    std::vector<uint16_t> dst(static_cast<size_t>(ds) * (h / 2), 0);
    demosaic_binned_2x2(src.data(), w, h, stride, Bayer::RGGB, dst.data(), ds);
    for (int i = 0; i < (h / 2); ++i)
        for (int j = 0; j < (w / 2); ++j) {
            const uint16_t* px = &dst[static_cast<size_t>(i) * ds + j * 3];
            assert(px[0] == 100 && px[1] == 200 && px[2] == 300);
        }
}

static void test_green_site_axes() {
    // RGGB, pixel (0,1) is G on a red row: R comes from the horizontal axis.
    const int w = 4, h = 4, stride = 4;
    std::vector<uint16_t> src(static_cast<size_t>(stride) * h, 0);
    // Row 0: R G R G -> put 40 and 80 on the two R neighbours of (0,1).
    src[0] = 40; src[2] = 80;
    const int ds = w * 3;
    std::vector<uint16_t> dst(static_cast<size_t>(ds) * h, 0);
    demosaic_bilinear(src.data(), w, h, stride, Bayer::RGGB, dst.data(), ds);
    assert(dst[1 * 3 + 0] == 60);   // (40 + 80) / 2
}

int main() {
    test_cfa_color_table();
    test_reflect_preserves_parity();
    test_flat_field_all_patterns();
    test_binning_flat();
    test_green_site_axes();
    std::cout << "bayer_demosaic: ok\n";
    return 0;
}
