// Part 5: std::move and perfect forwarding.

#include <iostream>
#include <utility>

void process(int&) { std::cout << "  process(int&)\n"; }
void process(int&&) { std::cout << "  process(int&&)\n"; }

template <typename T>
void forwarder(T&& arg) {
    process(std::forward<T>(arg));
}

struct Heavy {
    Heavy() { std::cout << "  Heavy ctor\n"; }
    Heavy(Heavy&&) { std::cout << "  Heavy move ctor\n"; }
};

Heavy make_heavy() { return Heavy{}; }

int main() {
    std::cout << "std::move only casts:\n";
    int a = 1;
    std::cout << "  before move a=" << a << "\n";
    process(std::move(a));  // still prints int& if overload is int& — actually int&& wins for rvalue ref from move
    // For int, std::move(a) is int&& but a is lvalue... move gives xvalue binding to int&&

    std::cout << "perfect forwarding:\n";
    int b = 2;
    forwarder(b);   // T=int&  → forward → int&
    forwarder(3);   // T=int   → forward → int&&

    std::cout << "RVO on return:\n";
    (void)make_heavy();

    std::cout << "05_move_forwarding: ok\n";
    return 0;
}
