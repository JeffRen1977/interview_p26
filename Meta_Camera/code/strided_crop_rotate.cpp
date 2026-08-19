// 2D strided crop / rotate 90 / flip — Meta Camera coding drill.
//
// ISP output is never tightly packed: stride != width * bpp. Every loop below
// walks rows via `base + r * stride`, never `base[r * width + c]`.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Crop: pure pointer math on the source, real copy into a packed dst.
// ---------------------------------------------------------------------------

// Returns a pointer to the top-left pixel of the ROI. No copy at all.
// Only valid when the consumer also accepts `src_stride`.
const uint8_t* crop_view(const uint8_t* src, int src_stride, int x, int y,
                         int bpp) {
    return src + static_cast<size_t>(y) * src_stride + static_cast<size_t>(x) * bpp;
}

// Copy a (w x h) ROI at (x, y) out of a strided image.
void crop_copy(const uint8_t* src, int src_stride, uint8_t* dst, int dst_stride,
               int x, int y, int w, int h, int bpp) {
    const uint8_t* s = crop_view(src, src_stride, x, y, bpp);
    for (int r = 0; r < h; ++r) {
        std::memcpy(dst + static_cast<size_t>(r) * dst_stride,
                    s + static_cast<size_t>(r) * src_stride,
                    static_cast<size_t>(w) * bpp);
    }
}

// ---------------------------------------------------------------------------
// 2. Rotate 90 clockwise. dst is (height x width): dst_w = h, dst_h = w.
//    dst(r, c) = src(height - 1 - c, r)
// ---------------------------------------------------------------------------

void rotate90_cw_u8(const uint8_t* src, int width, int height, int src_stride,
                    uint8_t* dst, int dst_stride) {
    for (int r = 0; r < height; ++r) {
        const uint8_t* src_row = src + static_cast<size_t>(r) * src_stride;
        for (int c = 0; c < width; ++c) {
            // src(r, c) lands at dst(c, height - 1 - r)
            dst[static_cast<size_t>(c) * dst_stride + (height - 1 - r)] = src_row[c];
        }
    }
}

void rotate90_ccw_u8(const uint8_t* src, int width, int height, int src_stride,
                     uint8_t* dst, int dst_stride) {
    for (int r = 0; r < height; ++r) {
        const uint8_t* src_row = src + static_cast<size_t>(r) * src_stride;
        for (int c = 0; c < width; ++c) {
            // src(r, c) lands at dst(width - 1 - c, r)
            dst[static_cast<size_t>(width - 1 - c) * dst_stride + r] = src_row[c];
        }
    }
}

// Cache-friendly variant: tile the rotation so the scattered writes stay
// inside one or two cache lines. TILE=32 -> 32x32 u8 block = 1 KB.
void rotate90_cw_tiled_u8(const uint8_t* src, int width, int height,
                          int src_stride, uint8_t* dst, int dst_stride) {
    const int TILE = 32;
    for (int r0 = 0; r0 < height; r0 += TILE) {
        for (int c0 = 0; c0 < width; c0 += TILE) {
            const int r1 = (r0 + TILE < height) ? r0 + TILE : height;
            const int c1 = (c0 + TILE < width) ? c0 + TILE : width;
            for (int r = r0; r < r1; ++r) {
                const uint8_t* src_row = src + static_cast<size_t>(r) * src_stride;
                for (int c = c0; c < c1; ++c) {
                    dst[static_cast<size_t>(c) * dst_stride + (height - 1 - r)] =
                        src_row[c];
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 3. Flips. Horizontal flip is in-place safe (swap within a row).
//    Vertical flip is in-place safe (swap whole rows).
// ---------------------------------------------------------------------------

void flip_horizontal_u8_inplace(uint8_t* img, int width, int height, int stride) {
    for (int r = 0; r < height; ++r) {
        uint8_t* row = img + static_cast<size_t>(r) * stride;
        for (int lo = 0, hi = width - 1; lo < hi; ++lo, --hi) {
            const uint8_t t = row[lo];
            row[lo] = row[hi];
            row[hi] = t;
        }
    }
}

void flip_vertical_u8_inplace(uint8_t* img, int width, int height, int stride) {
    std::vector<uint8_t> tmp(static_cast<size_t>(width));
    for (int lo = 0, hi = height - 1; lo < hi; ++lo, --hi) {
        uint8_t* a = img + static_cast<size_t>(lo) * stride;
        uint8_t* b = img + static_cast<size_t>(hi) * stride;
        std::memcpy(tmp.data(), a, static_cast<size_t>(width));
        std::memcpy(a, b, static_cast<size_t>(width));
        std::memcpy(b, tmp.data(), static_cast<size_t>(width));
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// 4x3 image, stride 8 (4 bytes of row padding):
//   0  1  2  3
//  10 11 12 13
//  20 21 22 23
static std::vector<uint8_t> make_src(int w, int h, int stride) {
    std::vector<uint8_t> v(static_cast<size_t>(stride) * h, 0xEE);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c)
            v[static_cast<size_t>(r) * stride + c] = static_cast<uint8_t>(r * 10 + c);
    return v;
}

static void test_crop() {
    const int w = 4, h = 3, stride = 8;
    auto src = make_src(w, h, stride);
    // ROI (x=1, y=1, 2x2) -> {11,12, 21,22}
    uint8_t dst[2 * 5];
    std::memset(dst, 0, sizeof(dst));
    crop_copy(src.data(), stride, dst, 5, 1, 1, 2, 2, 1);
    assert(dst[0] == 11 && dst[1] == 12);
    assert(dst[5] == 21 && dst[6] == 22);
    // A zero-copy view sees the same first pixel.
    assert(*crop_view(src.data(), stride, 1, 1, 1) == 11);
}

static void test_rotate_cw() {
    const int w = 4, h = 3, stride = 8;
    auto src = make_src(w, h, stride);
    const int dw = h, dh = w, dstride = 6;  // dst is 3 wide, 4 tall
    std::vector<uint8_t> dst(static_cast<size_t>(dstride) * dh, 0);
    rotate90_cw_u8(src.data(), w, h, stride, dst.data(), dstride);
    // Expected:
    //  20 10  0
    //  21 11  1
    //  22 12  2
    //  23 13  3
    const uint8_t want[4][3] = {{20, 10, 0}, {21, 11, 1}, {22, 12, 2}, {23, 13, 3}};
    for (int r = 0; r < dh; ++r)
        for (int c = 0; c < dw; ++c)
            assert(dst[static_cast<size_t>(r) * dstride + c] == want[r][c]);

    // Tiled variant must be bit-identical.
    std::vector<uint8_t> dst2(static_cast<size_t>(dstride) * dh, 0);
    rotate90_cw_tiled_u8(src.data(), w, h, stride, dst2.data(), dstride);
    assert(dst == dst2);
}

static void test_rotate_ccw_is_inverse() {
    const int w = 4, h = 3, stride = 8;
    auto src = make_src(w, h, stride);
    std::vector<uint8_t> mid(static_cast<size_t>(6) * w, 0);
    rotate90_cw_u8(src.data(), w, h, stride, mid.data(), 6);
    std::vector<uint8_t> back(static_cast<size_t>(stride) * h, 0);
    rotate90_ccw_u8(mid.data(), h, w, 6, back.data(), stride);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c)
            assert(back[static_cast<size_t>(r) * stride + c] ==
                   src[static_cast<size_t>(r) * stride + c]);
}

static void test_flips() {
    const int w = 4, h = 3, stride = 8;
    auto img = make_src(w, h, stride);
    flip_horizontal_u8_inplace(img.data(), w, h, stride);
    assert(img[0] == 3 && img[3] == 0);
    assert(img[stride + 0] == 13 && img[stride + 3] == 10);
    assert(img[4] == 0xEE);  // padding untouched

    auto img2 = make_src(w, h, stride);
    flip_vertical_u8_inplace(img2.data(), w, h, stride);
    assert(img2[0] == 20 && img2[static_cast<size_t>(2) * stride] == 0);
    assert(img2[4] == 0xEE);
}

int main() {
    test_crop();
    test_rotate_cw();
    test_rotate_ccw_is_inverse();
    test_flips();
    std::cout << "strided_crop_rotate: ok\n";
    return 0;
}
