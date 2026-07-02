// Part 9: alignment, padding, offsetof.

#include <cassert>
#include <cstddef>
#include <iostream>

struct A {
    char c;
    int i;
};

struct B {
    int i;
    char c;
};

struct C {
    char c1;
    char c2;
    int i;
};

#pragma pack(push, 1)
struct Packed {
    char c;
    int i;
};
#pragma pack(pop)

int main() {
    std::cout << "sizeof(A)=" << sizeof(A)
              << " offsetof(A::c)=" << offsetof(A, c)
              << " offsetof(A::i)=" << offsetof(A, i) << "\n";
    assert(sizeof(A) >= 5);
    // Typical: 8 bytes (1 char + 3 pad + 4 int)

    std::cout << "sizeof(B)=" << sizeof(B) << "\n";
    std::cout << "sizeof(C)=" << sizeof(C) << "\n";
    std::cout << "sizeof(Packed)=" << sizeof(Packed) << " (pragma pack)\n";
    std::cout << "alignof(int)=" << alignof(int) << "\n";

    std::cout << "08_memory_layout: ok\n";
    return 0;
}
