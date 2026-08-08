// Part 2: new vs malloc, placement new, delete[].

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

struct Widget {
    int value;
    explicit Widget(int v) : value(v) { std::cout << "  Widget(" << v << ")\n"; }
    ~Widget() { std::cout << "  ~Widget(" << value << ")\n"; }
};

void heap_new_delete() {
    std::cout << "new/delete:\n";
    auto* p = new Widget(42);
    assert(p->value == 42);
    delete p;
}

void malloc_no_ctor() {
    std::cout << "malloc (no constructor):\n";
    void* raw = std::malloc(sizeof(Widget));
    assert(raw != nullptr);
    // Widget* p = static_cast<Widget*>(raw);
    // p->value = 42;  // UB if used as Widget without constructor
    std::free(raw);
}

void placement_new_demo() {
    std::cout << "placement new:\n";
    alignas(Widget) unsigned char buf[sizeof(Widget)];
    auto* p = new (buf) Widget(99);
    assert(p->value == 99);
    p->~Widget();  // must destroy manually; buf is not freed by delete
}

void array_new_delete() {
    std::cout << "new[]/delete[]:\n";
    auto* arr = new Widget[3]{Widget(1), Widget(2), Widget(3)};
    delete[] arr;  // must use delete[], not delete
}

int main() {
    heap_new_delete();
    malloc_no_ctor();
    placement_new_demo();
    array_new_delete();
    std::cout << "02_memory_placement: ok\n";
    return 0;
}
