// Part 1: Rule of Five, copy vs move, RVO demonstration.

#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

class Buffer {
 public:
    explicit Buffer(size_t n) : data_(new int[n]), size_(n) {
        std::cout << "  ctor size=" << size_ << "\n";
    }

    ~Buffer() {
        std::cout << "  dtor size=" << size_ << "\n";
        delete[] data_;
    }

    Buffer(const Buffer& other) : data_(new int[other.size_]), size_(other.size_) {
        std::cout << "  copy ctor\n";
        for (size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
    }

    Buffer& operator=(const Buffer& other) {
        std::cout << "  copy assign\n";
        if (this != &other) {
            Buffer tmp(other);
            swap(tmp);
        }
        return *this;
    }

    Buffer(Buffer&& other) noexcept : data_(other.data_), size_(other.size_) {
        std::cout << "  move ctor\n";
        other.data_ = nullptr;
        other.size_ = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        std::cout << "  move assign\n";
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void swap(Buffer& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

 private:
    int* data_;
    size_t size_;
};

Buffer make_buffer() {
    return Buffer(4);  // RVO: typically no copy/move in C++17
}

void copy_vs_assign() {
    std::cout << "copy ctor:\n";
  Buffer a(2);
  Buffer b = a;  // copy constructor

    std::cout << "copy assign:\n";
  Buffer c(3);
  c = a;  // copy assignment
}

void move_demo() {
    std::cout << "move ctor:\n";
  Buffer x(2);
  Buffer y = std::move(x);  // move constructor

    std::cout << "move assign:\n";
  Buffer z(1);
  z = std::move(y);  // move assignment
}

void rvo_demo() {
    std::cout << "RVO:\n";
  Buffer buf = make_buffer();
  (void)buf;
}

int main() {
    copy_vs_assign();
    move_demo();
    rvo_demo();
    std::cout << "01_rule_of_five: ok\n";
    return 0;
}
