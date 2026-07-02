// Part 10: vector growth, reserve vs resize, unordered_map basics.

#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

void vector_growth() {
    std::vector<int> v;
    size_t last_cap = 0;
    std::cout << "vector push_back growth:\n";
    for (int i = 0; i < 10; ++i) {
        v.push_back(i);
        if (v.capacity() != last_cap) {
            std::cout << "  size=" << v.size() << " capacity=" << v.capacity() << "\n";
            last_cap = v.capacity();
        }
    }
}

void reserve_vs_resize() {
    std::vector<int> v;
    v.reserve(100);
    assert(v.size() == 0);
    assert(v.capacity() >= 100);

    v.resize(10);
    assert(v.size() == 10);
    std::cout << "reserve: capacity only; resize: changes size\n";
}

void unordered_map_demo() {
    std::unordered_map<std::string, int> freq;
    for (const char* word : {"aws", "ec2", "aws", "nitro"}) {
        ++freq[word];
    }
    assert(freq["aws"] == 2);
    std::cout << "unordered_map bucket_count=" << freq.bucket_count()
              << " load_factor=" << freq.load_factor() << "\n";
}

int main() {
    vector_growth();
    reserve_vs_resize();
    unordered_map_demo();
    std::cout << "09_stl_internals: ok\n";
    return 0;
}
