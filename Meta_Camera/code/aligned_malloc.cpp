// Aligned malloc / free — Meta Camera coding drill (DMA / SIMD buffers).
//
// Extra allocation: bytes + alignment - 1 + sizeof(void*)
// aligned = (raw + sizeof(void*) + alignment - 1) & ~(alignment - 1)
// Store raw immediately before aligned so aligned_free can std::free(raw).

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

static inline bool is_power_of_two(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

void* aligned_malloc(size_t bytes, size_t alignment) {
    if (bytes == 0 || !is_power_of_two(alignment)) {
        return nullptr;
    }
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    const size_t header = sizeof(void*);
    const size_t pad = alignment - 1;
    if (bytes > std::numeric_limits<size_t>::max() - pad - header) {
        return nullptr;
    }
    const size_t total_size = bytes + pad + header;

    void* raw_ptr = std::malloc(total_size);
    if (!raw_ptr) {
        return nullptr;
    }

    const uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
    const uintptr_t candidate = raw_addr + header;
    const uintptr_t aligned_addr =
        (candidate + pad) & ~static_cast<uintptr_t>(pad);
    void* aligned_ptr = reinterpret_cast<void*>(aligned_addr);

    reinterpret_cast<void**>(aligned_ptr)[-1] = raw_ptr;
    return aligned_ptr;
}

void aligned_free(void* aligned_ptr) {
    if (!aligned_ptr) {
        return;
    }
    void* raw_ptr = reinterpret_cast<void**>(aligned_ptr)[-1];
    std::free(raw_ptr);
}

static bool is_aligned(const void* p, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(p) & (alignment - 1)) == 0;
}

static void test_reject_bad_args() {
    assert(aligned_malloc(0, 16) == nullptr);
    assert(aligned_malloc(64, 0) == nullptr);
    assert(aligned_malloc(64, 24) == nullptr);  // not a power of two
    aligned_free(nullptr);
}

static void test_alignments() {
    const size_t alignments[] = {8, 16, 32, 64, 128};
    for (size_t alignment : alignments) {
        void* p = aligned_malloc(100, alignment);
        assert(p != nullptr);
        assert(is_aligned(p, alignment));
        std::memset(p, 0xAB, 100);
        assert(static_cast<unsigned char*>(p)[0] == 0xAB);
        assert(static_cast<unsigned char*>(p)[99] == 0xAB);
        aligned_free(p);
    }
}

static void test_bump_small_alignment() {
    void* p = aligned_malloc(32, 2);  // bumped to sizeof(void*)
    assert(p != nullptr);
    assert(is_aligned(p, sizeof(void*)));
    aligned_free(p);
}

static void test_hidden_raw_pointer() {
    void* p = aligned_malloc(16, 64);
    assert(p != nullptr);
    void* raw = reinterpret_cast<void**>(p)[-1];
    assert(raw != nullptr);
    assert(raw <= p);
    aligned_free(p);
}

static void test_many_allocs() {
    std::vector<void*> ptrs;
    for (int i = 0; i < 64; ++i) {
        void* p = aligned_malloc(17 + static_cast<size_t>(i), 64);
        assert(p != nullptr);
        assert(is_aligned(p, 64));
        ptrs.push_back(p);
    }
    for (void* p : ptrs) {
        aligned_free(p);
    }
}

int main() {
    test_reject_bad_args();
    test_alignments();
    test_bump_small_alignment();
    test_hidden_raw_pointer();
    test_many_allocs();
    std::cout << "aligned_malloc: ok\n";
    return 0;
}
