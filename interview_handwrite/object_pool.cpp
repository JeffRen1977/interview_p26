// Object pool — pre-allocated buffer reuse.
//
// Whiteboard talking points:
// - Avoid repeated heap alloc/free on a hot path.
// - acquire() marks a slot in_use; release() returns it to the pool.
// - Blocking acquire spins/sleeps when pool is exhausted.
// - Identity check on release prevents double-free style bugs.

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

template <typename T>
class ObjectPool {
 public:
    ObjectPool(std::function<T()> factory, size_t size) : items_(size) {
        for (size_t i = 0; i < size; ++i) {
            items_[i].value = factory();
            items_[i].in_use = false;
        }
    }

    T* acquire(bool block = true,
               std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& item : items_) {
                    if (!item.in_use) {
                        item.in_use = true;
                        return &item.value;
                    }
                }
            }

            if (!block) {
                return nullptr;
            }

            if (timeout != std::chrono::milliseconds::max()) {
                const auto remaining = deadline - std::chrono::steady_clock::now();
                if (remaining <= std::chrono::steady_clock::duration::zero()) {
                    return nullptr;
                }
                const auto sleep_for =
                    remaining < std::chrono::milliseconds(1) ? remaining
                                                           : std::chrono::milliseconds(1);
                std::this_thread::sleep_for(sleep_for);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void release(T* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : items_) {
            if (&item.value == obj) {
                item.in_use = false;
                return;
            }
        }
        throw std::invalid_argument("object does not belong to this pool");
    }

 private:
    struct PooledObject {
        T value{};
        bool in_use = false;
    };

    std::vector<PooledObject> items_;
    std::mutex mutex_;
};

bool test_object_pool() {
    ObjectPool<std::vector<char>> pool(
        [] { return std::vector<char>(1024); }, 2);

    auto* b1 = pool.acquire();
    auto* b2 = pool.acquire();
    assert(pool.acquire(false) == nullptr);

    pool.release(b1);
    auto* b3 = pool.acquire();
    assert(b3 == b1);

    pool.release(b2);
    pool.release(b3);
    return true;
}

int main() {
    assert(test_object_pool());
    std::cout << "object_pool: ok\n";
    return 0;
}
