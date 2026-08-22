// memcpy / memmove with overlap — Meta Camera coding drill.

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <vector>

void* my_memcpy(void* dest, const void* src, size_t n) {
    auto* d = static_cast<unsigned char*>(dest);
    const auto* s = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void* my_memmove(void* dest, const void* src, size_t n) {
    auto* d = static_cast<unsigned char*>(dest);
    const auto* s = static_cast<const unsigned char*>(src);
    if (d == s || n == 0) {
        return dest;
    }
    if (d < s || d >= s + n) {
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

static void test_non_overlap() {
    unsigned char src[] = {1, 2, 3, 4, 5};
    unsigned char dst[5] = {};
    my_memmove(dst, src, 5);
    assert(std::memcmp(dst, src, 5) == 0);
}

static void test_overlap_forward() {
    // dest < src: shift left. [1,2,3,4,5] → move 4 bytes from &buf[1] to &buf[0]
    unsigned char buf[] = {1, 2, 3, 4, 5};
    my_memmove(buf, buf + 1, 4);
    const unsigned char expect[] = {2, 3, 4, 5, 5};
    assert(std::memcmp(buf, expect, 5) == 0);
}

static void test_overlap_backward() {
    // dest > src: shift right.
    unsigned char buf[] = {1, 2, 3, 4, 5};
    my_memmove(buf + 1, buf, 4);
    const unsigned char expect[] = {1, 1, 2, 3, 4};
    assert(std::memcmp(buf, expect, 5) == 0);
}

static void test_matches_libc() {
    std::vector<unsigned char> a(32);
    std::vector<unsigned char> b(32);
    for (int n = 0; n <= 16; ++n) {
        for (int src = 0; src <= 16; ++src) {
            for (int dst = 0; dst <= 16; ++dst) {
                for (int i = 0; i < 32; ++i) {
                    a[static_cast<size_t>(i)] = b[static_cast<size_t>(i)] =
                        static_cast<unsigned char>(i + 1);
                }
                std::memmove(a.data() + dst, a.data() + src, static_cast<size_t>(n));
                my_memmove(b.data() + dst, b.data() + src, static_cast<size_t>(n));
                assert(a == b);
            }
        }
    }
}

static void test_memcpy_disjoint() {
    unsigned char src[] = {9, 8, 7};
    unsigned char dst[3] = {};
    my_memcpy(dst, src, 3);
    assert(dst[0] == 9 && dst[2] == 7);
}

int main() {
    test_non_overlap();
    test_overlap_forward();
    test_overlap_backward();
    test_matches_libc();
    test_memcpy_disjoint();
    std::cout << "memmove: ok\n";
    return 0;
}
