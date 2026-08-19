// 8-bit histogram + CDF equalization with stride — Meta Camera coding drill.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

void histogram_u8(const uint8_t* src, int width, int height, int src_stride,
                  uint32_t hist[256]) {
    std::memset(hist, 0, 256 * sizeof(uint32_t));
    for (int r = 0; r < height; ++r) {
        const uint8_t* row = src + r * src_stride;
        for (int c = 0; c < width; ++c) {
            ++hist[row[c]];
        }
    }
}

void cdf_from_hist(const uint32_t hist[256], uint32_t cdf[256]) {
    uint32_t acc = 0;
    for (int i = 0; i < 256; ++i) {
        acc += hist[i];
        cdf[i] = acc;
    }
}

void equalize_u8(const uint8_t* src, uint8_t* dst, int width, int height,
                 int src_stride, int dst_stride) {
    uint32_t hist[256];
    uint32_t cdf[256];
    histogram_u8(src, width, height, src_stride, hist);
    cdf_from_hist(hist, cdf);

    const uint32_t n = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
    uint32_t cdf_min = 0;
    for (int i = 0; i < 256; ++i) {
        if (cdf[i] != 0) {
            cdf_min = cdf[i];
            break;
        }
    }

    uint8_t lut[256];
    if (n == 0 || n == cdf_min) {
        for (int i = 0; i < 256; ++i) {
            lut[i] = static_cast<uint8_t>(i);
        }
    } else {
        for (int i = 0; i < 256; ++i) {
            const uint32_t num = (cdf[i] - cdf_min) * 255u;
            lut[i] = static_cast<uint8_t>((num + (n - cdf_min) / 2) / (n - cdf_min));
        }
    }

    for (int r = 0; r < height; ++r) {
        const uint8_t* srow = src + r * src_stride;
        uint8_t* drow = dst + r * dst_stride;
        for (int c = 0; c < width; ++c) {
            drow[c] = lut[srow[c]];
        }
    }
}

static void test_hist_ignores_stride_padding() {
    const int w = 2, h = 2, stride = 4;
    uint8_t src[] = {
        1, 2, 9, 9,
        3, 4, 9, 9,
    };
    uint32_t hist[256] = {};
    histogram_u8(src, w, h, stride, hist);
    assert(hist[1] == 1 && hist[2] == 1 && hist[3] == 1 && hist[4] == 1);
    assert(hist[9] == 0);
    uint32_t cdf[256];
    cdf_from_hist(hist, cdf);
    assert(cdf[4] == 4);
    assert(cdf[255] == 4);
}

static void test_equalize_constant() {
    uint8_t src[4] = {40, 40, 40, 40};
    uint8_t dst[4] = {};
    equalize_u8(src, dst, 2, 2, 2, 2);
    assert(dst[0] == 40 && dst[3] == 40);
}

static void test_equalize_two_levels() {
    uint8_t src[4] = {0, 0, 255, 255};
    uint8_t dst[4] = {};
    equalize_u8(src, dst, 2, 2, 2, 2);
    assert(dst[0] == 0);
    assert(dst[2] == 255);
}

int main() {
    test_hist_ignores_stride_padding();
    test_equalize_constant();
    test_equalize_two_levels();
    std::cout << "histogram: ok\n";
    return 0;
}
