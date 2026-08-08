// Part 8: virtual dispatch, virtual destructor.

#include <cassert>
#include <iostream>

struct Base {
    virtual void foo() const { std::cout << "  Base::foo\n"; }
    virtual ~Base() { std::cout << "  ~Base\n"; }
    int bx = 1;
};

struct Derived : Base {
    void foo() const override { std::cout << "  Derived::foo\n"; }
    ~Derived() override { std::cout << "  ~Derived\n"; }
    int dy = 2;
};

void virtual_call() {
    Derived d;
    Base& ref = d;
    ref.foo();  // Derived::foo via vtable

    Base* ptr = new Derived();
    ptr->foo();
    delete ptr;  // requires virtual ~Base to call ~Derived
}

void non_virtual_trap() {
    struct BadBase {
        ~BadBase() { std::cout << "  ~BadBase only\n"; }
    };
    struct BadDerived : BadBase {
        ~BadDerived() { std::cout << "  ~BadDerived\n"; }
    };
    std::cout << "Without virtual dtor (stack object ok):\n";
    BadDerived bd;
}

int main() {
    virtual_call();
    non_virtual_trap();
    std::cout << "sizeof(Base)=" << sizeof(Base)
              << " sizeof(Derived)=" << sizeof(Derived) << "\n";
    std::cout << "  (Derived larger: vptr + members + padding)\n";
    std::cout << "07_virtual_vtable: ok\n";
    return 0;
}
