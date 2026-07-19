// Hands-on: minimal vector (Rule of Five + grow).
// Fixes vs naive draft:
// - ctor is reserve-style: size_=0 (no fake uninitialized elements)
// - push_back: capacity 0 → grow to 1 (avoid 0*2 deadlock)
// - operator=: allocate-then-delete (exception-safe)

#include <cstddef>
#include <utility>

class vector_low_level {
 public:
    // Empty vector; optional initial capacity (size stays 0).
    explicit vector_low_level(std::size_t capacity = 0)
        : size_(0),
          capacity_(capacity),
          data_(capacity_ ? new int[capacity_] : nullptr) {}

    ~vector_low_level() { delete[] data_; }

    vector_low_level(const vector_low_level& other)
        : size_(other.size_),
          capacity_(other.capacity_),
          data_(capacity_ ? new int[capacity_] : nullptr) {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    vector_low_level& operator=(const vector_low_level& other) {
        if (this == &other) {
            return *this;
        }
        // Allocate first so a throwing new leaves *this unchanged.
        int* neu = other.capacity_ ? new int[other.capacity_] : nullptr;
        for (std::size_t i = 0; i < other.size_; ++i) {
            neu[i] = other.data_[i];
        }
        delete[] data_;
        data_ = neu;
        size_ = other.size_;
        capacity_ = other.capacity_;
        return *this;
    }

    vector_low_level(vector_low_level&& other) noexcept
        : size_(other.size_),
          capacity_(other.capacity_),
          data_(other.data_) {
        other.size_ = 0;
        other.capacity_ = 0;
        other.data_ = nullptr;
    }

    vector_low_level& operator=(vector_low_level&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        delete[] data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        data_ = other.data_;
        other.size_ = 0;
        other.capacity_ = 0;
        other.data_ = nullptr;
        return *this;
    }

    std::size_t get_size() const { return size_; }
    std::size_t get_capacity() const { return capacity_; }

    int& operator[](std::size_t i) { return data_[i]; }
    const int& operator[](std::size_t i) const { return data_[i]; }

    void push_back(int value) {
        if (size_ == capacity_) {
            const std::size_t new_cap = capacity_ == 0 ? 1 : capacity_ * 2;
            int* neu = new int[new_cap];
            for (std::size_t i = 0; i < size_; ++i) {
                neu[i] = data_[i];
            }
            delete[] data_;
            data_ = neu;
            capacity_ = new_cap;
        }
        data_[size_++] = value;
    }

 private:
    std::size_t size_;
    std::size_t capacity_;
    int* data_;
};
