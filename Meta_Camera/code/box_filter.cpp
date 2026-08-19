// 3x3 box / Gaussian filter with replicate padding + stride — Meta Camera drill.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static int clampi(int v, int lo, int hi) {
    return std::max(lo, std::min(v, hi));
}

void box_filter_3x3(const uint8_t* src, uint8_t* dst, int width, int height,
                    int src_stride, int dst_stride) {
    for (int r = 0; r < height; ++r) {
        uint8_t* dst_row = dst + r * dst_stride;
        for (int c = 0; c < width; ++c) {
            int sum = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int y = clampi(r + dy, 0, height - 1);
                const uint8_t* src_row = src + y * src_stride;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = clampi(c + dx, 0, width - 1);
                    sum += src_row[x];
                }
            }
            dst_row[c] = static_cast<uint8_t>(sum / 9);
        }
    }
}

void gaussian_filter_3x3(const uint8_t* src, uint8_t* dst, int width, int height,
                         int src_stride, int dst_stride) {
    static const int k[3][3] = {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}};
    for (int r = 0; r < height; ++r) {
        uint8_t* dst_row = dst + r * dst_stride;
        for (int c = 0; c < width; ++c) {
            int sum = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int y = clampi(r + dy, 0, height - 1);
                const uint8_t* src_row = src + y * src_stride;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = clampi(c + dx, 0, width - 1);
                    sum += src_row[x] * k[dy + 1][dx + 1];
                }
            }
            dst_row[c] = static_cast<uint8_t>(sum / 16);
        }
    }
}

static void test_constant() {
    const int w = 4, h = 3, stride = 8;
    std::vector<uint8_t> src(static_cast<size_t>(stride * h), 0);
    std::vector<uint8_t> dst(static_cast<size_t>(stride * h), 0xFF);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            src[static_cast<size_t>(r * stride + c)] = 90;
        }
    }
    box_filter_3x3(src.data(), dst.data(), w, h, stride, stride);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            assert(dst[static_cast<size_t>(r * stride + c)] == 90);
        }
    }
}

static void test_known_center() {
    // 3x3: center 255, rest 0 → sum=255, 255/9=28
    uint8_t src[9] = {0, 0, 0, 0, 255, 0, 0, 0, 0};
    uint8_t dst[9] = {};
    box_filter_3x3(src, dst, 3, 3, 3, 3);
    assert(dst[4] == 28);
}

static void test_gaussian_impulse() {
    uint8_t src[9] = {0, 0, 0, 0, 255, 0, 0, 0, 0};
    uint8_t dst[9] = {};
    gaussian_filter_3x3(src, dst, 3, 3, 3, 3);
    assert(dst[4] == static_cast<uint8_t>((255 * 4) / 16));  // 63
}

int main() {
    test_constant();
    test_known_center();
    test_gaussian_impulse();
    std::cout << "box_filter: ok\n";
    return 0;
}
