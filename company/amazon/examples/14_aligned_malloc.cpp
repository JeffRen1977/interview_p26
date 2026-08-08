// Aligned malloc/free — store original pointer just before the aligned address.
//
// aligned_malloc(size, alignment):
//   1) alignment must be power of 2
//   2) allocate size + alignment - 1 + sizeof(void*)
// 3) bump raw up to an aligned address that still has room for a hidden void*
//   4) *(aligned - sizeof(void*)) = raw
//
// aligned_free(p): free(*(void**)(p - sizeof(void*)))

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {

bool is_pow2(std::size_t x) { return x != 0 && (x & (x - 1)) == 0; }

}  // namespace

void* aligned_malloc(std::size_t size, std::size_t alignment) {
    if (size == 0 || !is_pow2(alignment)) {
        return nullptr;
    }

    // Extra: alignment-1 to reach next boundary + sizeof(void*) to stash raw.
    const std::size_t total = size + alignment - 1 + sizeof(void*);
    void* raw = std::malloc(total);
    if (raw == nullptr) {
        return nullptr;
    }

    auto addr = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
    addr = (addr + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);

    void* aligned = reinterpret_cast<void*>(addr);
    // Hide the original malloc pointer immediately before the aligned block.
    *(reinterpret_cast<void**>(aligned) - 1) = raw;
    return aligned;
}

void aligned_free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    void* raw = *(reinterpret_cast<void**>(ptr) - 1);
    std::free(raw);
}

int main() {
    for (std::size_t align : {8u, 16u, 64u, 128u}) {
        void* p = aligned_malloc(100, align);
        assert(p != nullptr);
        assert((reinterpret_cast<std::uintptr_t>(p) % align) == 0);
        aligned_free(p);
    }
    assert(aligned_malloc(10, 3) == nullptr);  // not power of 2
    assert(aligned_malloc(0, 64) == nullptr);
    aligned_free(nullptr);  // must be safe

    std::cout << "14_aligned_malloc: ok\n";
    return 0;
}
