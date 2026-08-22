// Endianness detect + 16/32-bit byte swap — Meta Camera coding drill.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

bool is_little_endian() {
    const uint16_t x = 0x0001;
    return *reinterpret_cast<const uint8_t*>(&x) == 0x01;
}

constexpr uint16_t bswap16(uint16_t x) {
    return static_cast<uint16_t>((x << 8) | (x >> 8));
}

constexpr uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) |
           ((x & 0x00FF0000u) >> 8) | ((x & 0xFF000000u) >> 24);
}

uint32_t to_big_endian32(uint32_t host) {
    return is_little_endian() ? bswap32(host) : host;
}

uint32_t from_big_endian32(uint32_t be) {
    return is_little_endian() ? bswap32(be) : be;
}

static void test_detect() {
    const uint32_t v = 0x12345678u;
    uint8_t bytes[4];
    std::memcpy(bytes, &v, 4);
    if (is_little_endian()) {
        assert(bytes[0] == 0x78 && bytes[3] == 0x12);
    } else {
        assert(bytes[0] == 0x12 && bytes[3] == 0x78);
    }
}

static void test_swap() {
    assert(bswap16(0x1234) == 0x3412);
    assert(bswap32(0x12345678u) == 0x78563412u);
    assert(bswap32(bswap32(0xA5A5A5A5u)) == 0xA5A5A5A5u);
    assert(bswap32(0u) == 0u);
    assert(from_big_endian32(to_big_endian32(0xDEADBEEFu)) == 0xDEADBEEFu);
}

int main() {
    test_detect();
    test_swap();
    std::cout << "endianness: ok (host is " << (is_little_endian() ? "LE" : "BE")
              << ")\n";
    return 0;
}
