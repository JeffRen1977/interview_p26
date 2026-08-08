// Part 3: const pointer combinations.

#include <cassert>
#include <iostream>

int main() {
    int x = 1, y = 2;

    const int* p1 = &x;  // pointer to const int
    (void)p1;
    // *p1 = 3;          // error
    p1 = &y;              // ok: pointer itself is mutable

    int* const p2 = &x;     // const pointer to int
    *p2 = 3;                // ok
    assert(x == 3);
    // p2 = &y;           // error

    const int* const p3 = &x;  // both const
    (void)p3;
    // *p3 = 4; p3 = &y;     // both error

    std::cout << "Reading rule: read from name outward.\n";
    std::cout << "  const int*     → *p is const\n";
    std::cout << "  int* const     → p is const\n";
    std::cout << "  const int* const → both const\n";
    std::cout << "03_const_pointers: ok\n";
    return 0;
}
