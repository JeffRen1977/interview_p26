// Part 7: unique_ptr, shared_ptr, weak_ptr, circular reference.

#include <cassert>
#include <iostream>
#include <memory>

struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> weak_next;  // breaks cycle
    int id;
    explicit Node(int i) : id(i) {}
    ~Node() { std::cout << "  ~Node(" << id << ")\n"; }
};

void unique_demo() {
    std::cout << "unique_ptr:\n";
    auto p = std::make_unique<int>(42);
    assert(*p == 42);
    // auto q = p;  // error: cannot copy
    auto q = std::move(p);
    assert(!p);
    assert(*q == 42);
}

void shared_demo() {
    std::cout << "shared_ptr:\n";
    auto a = std::make_shared<int>(10);
    std::shared_ptr<int> b = a;
    assert(a.use_count() == 2);
    b.reset();
    assert(a.use_count() == 1);
}

void circular_reference_leak() {
    std::cout << "circular reference (fixed with weak_ptr):\n";
    auto n1 = std::make_shared<Node>(1);
    auto n2 = std::make_shared<Node>(2);
    n1->weak_next = n2;
    n2->weak_next = n1;
    if (auto locked = n1->weak_next.lock()) {
        assert(locked->id == 2);
    }
}

struct Widget : std::enable_shared_from_this<Widget> {
    std::shared_ptr<Widget> getPtr() { return shared_from_this(); }
};

void enable_shared_demo() {
    std::cout << "enable_shared_from_this:\n";
    auto w = std::make_shared<Widget>();
    auto w2 = w->getPtr();
    assert(w.use_count() == 2);
}

int main() {
    unique_demo();
    shared_demo();
    circular_reference_leak();
    enable_shared_demo();
    std::cout << "06_smart_pointers: ok\n";
    return 0;
}
