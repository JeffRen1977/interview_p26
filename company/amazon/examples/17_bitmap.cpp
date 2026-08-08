// BitMap — 1 bit per key for membership tests.
//
// Interview classic: mark / query many integers with tiny memory vs hash set.
// Full uint32 space needs 2^32 bits = 512MB; here we demo a smaller universe.

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

class BitMap {
 public:
    explicit BitMap(std::uint64_t universe_size)
        : bits_((universe_size + 7) / 8, 0), universe_(universe_size) {}

    void set(std::uint64_t value) {
        assert(value < universe_);
        bits_[value >> 3] |= static_cast<std::uint8_t>(1u << (value & 7));
    }

    void clear(std::uint64_t value) {
        assert(value < universe_);
        bits_[value >> 3] &= static_cast<std::uint8_t>(~(1u << (value & 7)));
    }

    bool test(std::uint64_t value) const {
        assert(value < universe_);
        return (bits_[value >> 3] & (1u << (value & 7))) != 0;
    }

    std::size_t bytes() const { return bits_.size(); }

 private:
    std::vector<std::uint8_t> bits_;
    std::uint64_t universe_;
};

int main() {
    // 1M integers → 1M bits ≈ 128KB
    BitMap bm(1u << 20);
    assert(bm.bytes() == (1u << 20) / 8);

    assert(!bm.test(42));
    bm.set(42);
    assert(bm.test(42));
    bm.set(100);
    assert(bm.test(100));
    bm.clear(42);
    assert(!bm.test(42));
    assert(bm.test(100));

    // Oral math: 2^32 bits = 512MB for full 32-bit keyspace.
    std::cout << "17_bitmap: ok\n";
    return 0;
}
