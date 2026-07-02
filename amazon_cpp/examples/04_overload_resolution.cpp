// Part 4: lvalue/rvalue overload resolution.

#include <iostream>
#include <utility>

void foo(int& x) { std::cout << "  int&  (lvalue) value=" << x << "\n"; }
void foo(const int& x) { std::cout << "  const int& value=" << x << "\n"; }
void foo(int&& x) { std::cout << "  int&& (rvalue) value=" << x << "\n"; }

int main() {
    int x = 1;
    const int cx = 2;

    std::cout << "foo(x):           "; foo(x);              // int&
    std::cout << "foo(1):           "; foo(1);              // int&&
    std::cout << "foo(std::move(x)):"; foo(std::move(x));  // int&&
    std::cout << "foo(cx):          "; foo(cx);             // const int&

    std::cout << "04_overload_resolution: ok\n";
    return 0;
}
