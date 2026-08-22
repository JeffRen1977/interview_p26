// MIPI CSI-2 Packed RAW10 → uint16_t unpack (Meta Camera coding drill).
//
// 4 pixels × 10-bit = 40 bits = 5 bytes:
//   src[0..3] = P_i[9:2]
//   src[4]    = {P3[1:0], P2[1:0], P1[1:0], P0[1:0]}  (bits 7:6 … 1:0)
//
// P_i = (Byte_i << 2) | ((Byte_4 >> (2 * i)) & 0x03)
//
// Default output is LSB-aligned in [0, 1023]. Pass msb_align=true to << 6
// for 16-bit ISP pipelines that want the 10-bit value in bits [15:6].

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

void unpack_mipi_raw10(const uint8_t* __restrict src, uint16_t* __restrict dst,
                       size_t pixel_count, bool msb_align = false) {
    assert(pixel_count % 4 == 0);
    const size_t num_blocks = pixel_count / 4;
    const int msb_shift = msb_align ? 6 : 0;

    for (size_t i = 0; i < num_blocks; ++i) {
        const uint8_t b0 = src[0];
        const uint8_t b1 = src[1];
        const uint8_t b2 = src[2];
        const uint8_t b3 = src[3];
        const uint8_t b4 = src[4];

        dst[0] = static_cast<uint16_t>(((static_cast<uint16_t>(b0) << 2) | (b4 & 0x03))
                                       << msb_shift);
        dst[1] = static_cast<uint16_t>(((static_cast<uint16_t>(b1) << 2) | ((b4 >> 2) & 0x03))
                                       << msb_shift);
        dst[2] = static_cast<uint16_t>(((static_cast<uint16_t>(b2) << 2) | ((b4 >> 4) & 0x03))
                                       << msb_shift);
        dst[3] = static_cast<uint16_t>(((static_cast<uint16_t>(b3) << 2) | ((b4 >> 6) & 0x03))
                                       << msb_shift);
        src += 5;
        dst += 4;
    }
}

void unpack_raw10_frame(const uint8_t* src, uint16_t* dst, uint32_t width,
                        uint32_t height, uint32_t src_stride_bytes,
                        uint32_t dst_stride_pixels, bool msb_align = false) {
    assert(width % 4 == 0);
    const uint32_t packed_row = (width / 4) * 5;
    assert(src_stride_bytes >= packed_row);

    for (uint32_t r = 0; r < height; ++r) {
        const uint8_t* src_row = src + static_cast<size_t>(r) * src_stride_bytes;
        uint16_t* dst_row = dst + static_cast<size_t>(r) * dst_stride_pixels;
        unpack_mipi_raw10(src_row, dst_row, width, msb_align);
    }
}

void pack_mipi_raw10(const uint16_t* __restrict src, uint8_t* __restrict dst,
                     size_t pixel_count) {
    assert(pixel_count % 4 == 0);
    const size_t num_blocks = pixel_count / 4;

    for (size_t i = 0; i < num_blocks; ++i) {
        const uint16_t p0 = src[0] & 0x3FF;
        const uint16_t p1 = src[1] & 0x3FF;
        const uint16_t p2 = src[2] & 0x3FF;
        const uint16_t p3 = src[3] & 0x3FF;
        dst[0] = static_cast<uint8_t>(p0 >> 2);
        dst[1] = static_cast<uint8_t>(p1 >> 2);
        dst[2] = static_cast<uint8_t>(p2 >> 2);
        dst[3] = static_cast<uint8_t>(p3 >> 2);
        dst[4] = static_cast<uint8_t>((p0 & 0x03) | ((p1 & 0x03) << 2) |
                                     ((p2 & 0x03) << 4) | ((p3 & 0x03) << 6));
        src += 4;
        dst += 5;
    }
}

static void test_known_bytes() {
    // P = {0x3FF, 0x001, 0x2AA, 0x155}
    // 0x3FF = 11_1111_1111 → Byte0=0xFF, ls2=0b11
    // 0x001 = 00_0000_0001 → Byte1=0x00, ls2=0b01
    // 0x2AA = 10_1010_1010 → Byte2=0xAA, ls2=0b10
    // 0x155 = 01_0101_0101 → Byte3=0x55, ls2=0b01
    // Byte4 = 01 10 01 11 = 0x67
    const uint8_t src[] = {0xFF, 0x00, 0xAA, 0x55, 0x67};
    uint16_t dst[4] = {};
    unpack_mipi_raw10(src, dst, 4);
    assert(dst[0] == 0x3FF);
    assert(dst[1] == 0x001);
    assert(dst[2] == 0x2AA);
    assert(dst[3] == 0x155);
}

static void test_roundtrip() {
    const uint16_t pixels[] = {0, 1, 2, 3, 0x3FF, 0x200, 0x155, 0x2AA};
    uint8_t packed[10] = {};
    uint16_t out[8] = {};
    pack_mipi_raw10(pixels, packed, 8);
    unpack_mipi_raw10(packed, out, 8);
    for (int i = 0; i < 8; ++i) {
        assert(out[i] == pixels[i]);
    }
}

static void test_msb_align() {
    const uint8_t src[] = {0xFF, 0x00, 0xAA, 0x55, 0x67};
    uint16_t dst[4] = {};
    unpack_mipi_raw10(src, dst, 4, /*msb_align=*/true);
    assert(dst[0] == (0x3FF << 6));
    assert(dst[1] == (0x001 << 6));
    assert(dst[2] == (0x2AA << 6));
    assert(dst[3] == (0x155 << 6));
}

static void test_strided_frame() {
    // 8×2 pixels, packed row = 10 bytes, but hardware pads stride to 16.
    const uint32_t width = 8;
    const uint32_t height = 2;
    const uint32_t src_stride = 16;
    const uint32_t dst_stride = 8;
    std::vector<uint16_t> truth(width * height);
    for (uint32_t i = 0; i < truth.size(); ++i) {
        truth[i] = static_cast<uint16_t>(i & 0x3FF);
    }

    std::vector<uint8_t> src(src_stride * height, 0xCC);  // padding poison
    for (uint32_t r = 0; r < height; ++r) {
        pack_mipi_raw10(truth.data() + r * width, src.data() + r * src_stride, width);
    }

    std::vector<uint16_t> dst(dst_stride * height, 0xFFFF);
    unpack_raw10_frame(src.data(), dst.data(), width, height, src_stride, dst_stride);
    for (uint32_t i = 0; i < truth.size(); ++i) {
        assert(dst[i] == truth[i]);
    }
}

int main() {
    test_known_bytes();
    test_roundtrip();
    test_msb_align();
    test_strided_frame();
    std::cout << "unpack_mipi_raw10: ok\n";
    return 0;
}
