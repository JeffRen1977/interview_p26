// Min-heap (binary heap on a vector) — siftUp / siftDown.
//
// Network interview angle: schedule packets by timestamp / QoS priority.
// A min-heap always keeps the smallest key at index 0, so pop() is O(log n)
// extraction of the next-due item.
//
// Whiteboard talking points:
// - Store a complete binary tree in a contiguous array (great cache locality).
// - Index math: parent=(i-1)/2, left=2i+1, right=2i+2.
// - push: append at end, then siftUp (bubble toward root while smaller than parent).
// - pop:  save root, move last element to root, then siftDown (sink while larger
//         than the smaller child).
// - Heap property: every parent <= both children (min-heap). Not fully sorted.

#include <cassert>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

class MinHeap {
 public:
    // Amortized O(1) append + O(log n) siftUp.
    void push(int value) {
        // New leaf sits at the end of the complete tree (next open slot).
        data_.push_back(value);
        sift_up(data_.size() - 1);
    }

    // O(log n). Returns and removes the current minimum.
    int pop() {
        assert(!empty());
        const int top = data_.front();  // root = minimum

        // Fill the hole at the root with the last leaf so the shape stays
        // a complete binary tree (compact array, no gaps).
        data_.front() = data_.back();
        data_.pop_back();

        // Only sift if anything remains; a single-element heap is already valid.
        if (!empty()) {
            sift_down(0);
        }
        return top;
    }

    // O(1) peek — does not modify the heap.
    int top() const {
        assert(!empty());
        return data_.front();
    }

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

 private:
    // Tree navigation via arithmetic — no explicit child/parent pointers.
    static std::size_t parent(std::size_t i) { return (i - 1) / 2; }
    static std::size_t left(std::size_t i) { return 2 * i + 1; }
    static std::size_t right(std::size_t i) { return 2 * i + 2; }

    // Restore heap property after inserting at index i (a leaf).
    // Walk toward the root: while node < parent, swap and continue.
    void sift_up(std::size_t i) {
        while (i > 0) {
            const std::size_t p = parent(i);
            // Parent already <= child → path to root is fine; stop.
            if (data_[p] <= data_[i]) {
                break;
            }
            std::swap(data_[p], data_[i]);
            i = p;
        }
    }

    // Restore heap property after replacing the root (or any internal node).
    // Walk toward the leaves: swap with the smaller child until in order.
    void sift_down(std::size_t i) {
        while (true) {
            std::size_t best = i;  // candidate for the smallest among {i,L,R}
            const std::size_t l = left(i);
            const std::size_t r = right(i);

            // Prefer the smaller of the existing children.
            if (l < data_.size() && data_[l] < data_[best]) {
                best = l;
            }
            if (r < data_.size() && data_[r] < data_[best]) {
                best = r;
            }

            // Already smaller than both children (or is a leaf) → done.
            if (best == i) {
                break;
            }
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }

    // Example layout after pushing {5,1,4,2,3}:
    //   array: [1, 2, 4, 5, 3]
    //   tree:       1
    //             /   \
    //            2     4
    //           / \
    //          5   3
    std::vector<int> data_;
};

int main() {
    MinHeap h;

    // Build phase — each push may siftUp:
    //   push 5 → [5]
    //   push 1 → [1,5]          (1 sifted over 5)
    //   push 4 → [1,5,4]
    //   push 2 → [1,2,4,5]      (2 sifted over 5)
    //   push 3 → [1,2,4,5,3]
    for (int x : {5, 1, 4, 2, 3}) {
        h.push(x);
    }
    assert(h.size() == 5);
    assert(h.top() == 1);

    // Extract phase — each pop yields the next minimum:
    //   1, then 2, then 3, then 4, then 5  (heap sort ascending)
    std::vector<int> out;
    while (!h.empty()) {
        out.push_back(h.pop());
    }
    assert((out == std::vector<int>{1, 2, 3, 4, 5}));

    std::cout << "16_min_heap: ok\n";
    return 0;
}
