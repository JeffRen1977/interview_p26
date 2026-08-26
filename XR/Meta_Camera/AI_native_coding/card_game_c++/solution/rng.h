// Deterministic RNG. Given -- do not change this file.
//
// std::uniform_int_distribution is NOT portable across standard libraries, so
// this uses an explicit modulo. Same seed always produces the same sequence.
#pragma once

#include <random>

class Rng {
public:
    explicit Rng(unsigned seed) : engine_(seed) {}

    // Inclusive on both ends.
    int next_int(int lo, int hi) {
        const unsigned span = static_cast<unsigned>(hi - lo + 1);
        return lo + static_cast<int>(engine_() % span);
    }

private:
    std::mt19937 engine_;
};
