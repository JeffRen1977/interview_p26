// Cost/pressure analysis and two classic optimizations. Reference implementation.
#pragma once

#include "optimizer_utils.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

struct Metrics {
    long long time = 0;
    int memory = 0;

    bool operator==(const Metrics& other) const {
        return time == other.time && memory == other.memory;
    }
    bool operator!=(const Metrics& other) const { return !(*this == other); }
};

// Runtime work and peak live temporaries of `program`.
//
// time
//     Sum of costs.at(instr.op) over every instruction that has an operator.
//     A plain copy (`v = x`) costs nothing. An operator with no entry in
//     `costs` throws std::out_of_range.
//
// memory
//     Peak number of *computed* values alive at the same time. Only assignment
//     targets count -- input variables (never assigned) and integer literals
//     are free.
//
//     A computed value is alive immediately after the instruction that defines
//     it, and stays alive until the last instruction that reads it. `res` is
//     the program output, so it stays alive to the end. Concretely, `memory`
//     is the maximum over i of:
//
//         |{ v : v defined at some j <= i,
//                and (v == kOutput or v is read at some k > i) }|
//
//     A target that is never read and is not `res` therefore costs time but
//     contributes nothing to memory.
//
// An empty program is Metrics{0, 0}.
Metrics analyze(const std::vector<Instr>& program, const std::map<std::string, int>& costs);

// Drop every assignment whose target cannot be reached from `res`.
//
// Reachability runs backwards over the def-use graph and must reach a fixed
// point: removing one assignment can make its operands dead too. Literal
// operands keep nothing alive. The surviving instructions keep their original
// relative order. A program with no `res` at all optimizes to {}.
std::vector<Instr> eliminate_dead_code(const std::vector<Instr>& program);

// Evaluate whatever is known at compile time and propagate it forward.
//
// Walk the program in order, tracking which targets hold a known constant.
// Substitute those operands with their literal value. When both operands of an
// instruction are literals, evaluate it and emit `target = <value>` in its
// place, which makes the constant available to later instructions too.
//
// Arithmetic is 64-bit: literals are parsed with std::stoll and folded into
// long long, so `2000000000 + 2000000000` folds instead of wrapping an int.
//
// Division truncates toward zero, which is what C++ integer division already
// does -- do NOT route it through doubles or std::floor.
//
// Division by a literal zero is not folded: emit the instruction unchanged and
// stop treating its target as known.
//
// Instruction order and targets are preserved; only the right-hand sides
// change. Nothing is removed here -- that is eliminate_dead_code's job.
std::vector<Instr> fold_constants(const std::vector<Instr>& program);

// Fold first, then eliminate dead code, then measure what is left.
//
// The order matters: folding turns uses of a variable into literals, which is
// exactly what makes the assignment that produced it dead.
std::pair<std::vector<Instr>, Metrics> optimize(const std::vector<Instr>& program,
                                                const std::map<std::string, int>& costs);
