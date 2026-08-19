// NV12 (YUV420sp) -> RGB888 with stride and correct chroma siting.
// Meta Camera coding drill. ISP back-end almost always hands you NV12.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

// NV12 layout:
//   Y  plane: height     rows of y_stride  bytes, y_stride  >= width
//   UV plane: height/2   rows of uv_stride bytes, uv_stride >= width  (U,V interleaved)
// One (u, v) pair covers a 2x2 block of luma: chroma index = (r/2, c/2).

static inline uint8_t clamp_u8(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// BT.601 "video range" (limited range, Y in [16,235], UV in [16,240]).
// Integer form, 8 fractional bits:
//   R = (298*(Y-16)             + 409*(V-128) + 128) >> 8
//   G = (298*(Y-16) - 100*(U-128) - 208*(V-128) + 128) >> 8
//   B = (298*(Y-16) + 516*(U-128)              + 128) >> 8
static inline void yuv_to_rgb_601_video(int y, int u, int v, uint8_t* out) {
    const int c = y - 16;
    const int d = u - 128;
    const int e = v - 128;
    out[0] = clamp_u8((298 * c + 409 * e + 128) >> 8);
    out[1] = clamp_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
    out[2] = clamp_u8((298 * c + 516 * d + 128) >> 8);
}

// BT.601 "full range" (JFIF / Android ImageFormat default on many HALs).
//   R = Y                + 1.402  *(V-128)
//   G = Y - 0.344136*(U-128) - 0.714136*(V-128)
//   B = Y + 1.772  *(U-128)
static inline void yuv_to_rgb_601_full(int y, int u, int v, uint8_t* out) {
    const int d = u - 128;
    const int e = v - 128;
    out[0] = clamp_u8(y + ((359 * e + 128) >> 8));
    out[1] = clamp_u8(y - ((88 * d + 183 * e + 128) >> 8));
    out[2] = clamp_u8(y + ((454 * d + 128) >> 8));
}

enum class NvOrder { NV12, NV21 };   // NV12 = U,V ; NV21 = V,U
enum class YuvRange { Video, Full };

void nv12_to_rgb888(const uint8_t* y_plane, int y_stride,
                    const uint8_t* uv_plane, int uv_stride,
                    int width, int height,
                    uint8_t* rgb, int rgb_stride,
                    NvOrder order = NvOrder::NV12,
                    YuvRange range = YuvRange::Video) {
    const int u_off = (order == NvOrder::NV12) ? 0 : 1;
    const int v_off = (order == NvOrder::NV12) ? 1 : 0;

    for (int r = 0; r < height; ++r) {
        const uint8_t* y_row  = y_plane  + static_cast<size_t>(r) * y_stride;
        const uint8_t* uv_row = uv_plane + static_cast<size_t>(r >> 1) * uv_stride;
        uint8_t* dst_row = rgb + static_cast<size_t>(r) * rgb_stride;

        for (int c = 0; c < width; ++c) {
            const int uv_idx = (c >> 1) * 2;         // 2 bytes per chroma sample
            const int u = uv_row[uv_idx + u_off];
            const int v = uv_row[uv_idx + v_off];
            uint8_t* px = dst_row + static_cast<size_t>(c) * 3;
            if (range == YuvRange::Video) {
                yuv_to_rgb_601_video(y_row[c], u, v, px);
            } else {
                yuv_to_rgb_601_full(y_row[c], u, v, px);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Reverse direction: RGB -> NV12 (needed to build test vectors, and asked as a
// follow-up "how do you downsample chroma?"). Box-average the 2x2 luma block.
// ---------------------------------------------------------------------------
void rgb888_to_nv12(const uint8_t* rgb, int rgb_stride, int width, int height,
                    uint8_t* y_plane, int y_stride,
                    uint8_t* uv_plane, int uv_stride) {
    for (int r = 0; r < height; ++r) {
        const uint8_t* src = rgb + static_cast<size_t>(r) * rgb_stride;
        uint8_t* y_row = y_plane + static_cast<size_t>(r) * y_stride;
        for (int c = 0; c < width; ++c) {
            const int R = src[c * 3 + 0], G = src[c * 3 + 1], B = src[c * 3 + 2];
            y_row[c] = clamp_u8(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16);
        }
    }
    for (int r = 0; r < height; r += 2) {
        uint8_t* uv_row = uv_plane + static_cast<size_t>(r >> 1) * uv_stride;
        for (int c = 0; c < width; c += 2) {
            int sR = 0, sG = 0, sB = 0, n = 0;
            for (int dy = 0; dy < 2 && r + dy < height; ++dy) {
                const uint8_t* src = rgb + static_cast<size_t>(r + dy) * rgb_stride;
                for (int dx = 0; dx < 2 && c + dx < width; ++dx) {
                    sR += src[(c + dx) * 3 + 0];
                    sG += src[(c + dx) * 3 + 1];
                    sB += src[(c + dx) * 3 + 2];
                    ++n;
                }
            }
            const int R = sR / n, G = sG / n, B = sB / n;
            uv_row[(c >> 1) * 2 + 0] =
                clamp_u8(((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128);  // U
            uv_row[(c >> 1) * 2 + 1] =
                clamp_u8(((112 * R - 94 * G - 18 * B + 128) >> 8) + 128);   // V
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_gray_stays_gray() {
    // Y=126 (video range mid), U=V=128 -> neutral gray, R==G==B.
    const int w = 4, h = 4, ys = 8, uvs = 8, rs = 20;
    std::vector<uint8_t> y(static_cast<size_t>(ys) * h, 126);
    std::vector<uint8_t> uv(static_cast<size_t>(uvs) * (h / 2), 128);
    std::vector<uint8_t> rgb(static_cast<size_t>(rs) * h, 0);
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, rgb.data(), rs);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            const uint8_t* p = &rgb[static_cast<size_t>(r) * rs + c * 3];
            assert(p[0] == p[1] && p[1] == p[2]);
            assert(p[0] > 120 && p[0] < 135);
        }
    }
}

static void test_black_and_white_clamp() {
    const int w = 2, h = 2, ys = 4, uvs = 4, rs = 8;
    std::vector<uint8_t> y(static_cast<size_t>(ys) * h, 16);   // video-range black
    std::vector<uint8_t> uv(static_cast<size_t>(uvs), 128);
    std::vector<uint8_t> rgb(static_cast<size_t>(rs) * h, 0xFF);
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, rgb.data(), rs);
    assert(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);

    std::fill(y.begin(), y.end(), 235);                        // video-range white
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, rgb.data(), rs);
    assert(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
}

static void test_chroma_siting_shared_by_2x2() {
    // One chroma pair covers 4 luma samples: forcing V high must tint all
    // four pixels of the top-left 2x2 block red, and only that block.
    const int w = 4, h = 4, ys = 8, uvs = 8, rs = 16;
    std::vector<uint8_t> y(static_cast<size_t>(ys) * h, 126);
    std::vector<uint8_t> uv(static_cast<size_t>(uvs) * (h / 2), 128);
    uv[0] = 128;  // U
    uv[1] = 240;  // V of the (0,0) chroma sample -> red
    std::vector<uint8_t> rgb(static_cast<size_t>(rs) * h, 0);
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, rgb.data(), rs);
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 2; ++c) {
            const uint8_t* p = &rgb[static_cast<size_t>(r) * rs + c * 3];
            assert(p[0] > p[2] + 40);  // R clearly above B
        }
    const uint8_t* q = &rgb[2 * 3];    // pixel (0,2): different chroma sample
    assert(q[0] == q[1] && q[1] == q[2]);
}

static void test_nv21_swaps_u_and_v() {
    const int w = 2, h = 2, ys = 4, uvs = 4, rs = 8;
    std::vector<uint8_t> y(static_cast<size_t>(ys) * h, 126);
    std::vector<uint8_t> uv{128, 240, 0, 0};   // as NV12: U=128 V=240 -> red
    std::vector<uint8_t> a(static_cast<size_t>(rs) * h, 0), b = a;
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, a.data(), rs, NvOrder::NV12);
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, b.data(), rs, NvOrder::NV21);
    assert(a[0] > a[2]);   // NV12 reading -> red
    assert(b[2] > b[0]);   // NV21 reading -> blue
}

static void test_roundtrip_is_close() {
    const int w = 8, h = 8, rs = 8 * 3 + 4;
    std::vector<uint8_t> rgb(static_cast<size_t>(rs) * h, 0);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            uint8_t* p = &rgb[static_cast<size_t>(r) * rs + c * 3];
            p[0] = static_cast<uint8_t>(20 * r); p[1] = 100; p[2] = static_cast<uint8_t>(20 * c);
        }
    const int ys = 12, uvs = 12;
    std::vector<uint8_t> y(static_cast<size_t>(ys) * h), uv(static_cast<size_t>(uvs) * (h / 2));
    rgb888_to_nv12(rgb.data(), rs, w, h, y.data(), ys, uv.data(), uvs);
    std::vector<uint8_t> back(static_cast<size_t>(rs) * h, 0);
    nv12_to_rgb888(y.data(), ys, uv.data(), uvs, w, h, back.data(), rs);
    // 4:2:0 is lossy on chroma; tolerate a wide band, just prove it is not garbage.
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c)
            for (int k = 0; k < 3; ++k) {
                const int d = std::abs(int(rgb[static_cast<size_t>(r) * rs + c * 3 + k]) -
                                       int(back[static_cast<size_t>(r) * rs + c * 3 + k]));
                assert(d <= 40);
            }
}

int main() {
    test_gray_stays_gray();
    test_black_and_white_clamp();
    test_chroma_siting_shared_by_2x2();
    test_nv21_swaps_u_and_v();
    test_roundtrip_is_close();
    std::cout << "nv12_to_rgb: ok\n";
    return 0;
}
