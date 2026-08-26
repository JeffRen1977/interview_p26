#include "compiler_optimizer.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

// Truncate toward zero. C++ integer division already does this (guaranteed
// since C++11), so the only job here is to keep the arithmetic in long long
// and refuse the two cases that would be undefined.
std::optional<long long> apply_op(const std::string& op, long long a, long long b) {
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") {
        if (b == 0) return std::nullopt;
        // LLONG_MIN / -1 overflows and is UB; leave it for runtime.
        if (a == std::numeric_limits<long long>::min() && b == -1) return std::nullopt;
        return a / b;
    }
    throw std::invalid_argument("unknown operator " + op);
}

std::string resolve(const std::string& token,
                    const std::unordered_map<std::string, long long>& known) {
    const auto it = known.find(token);
    return it == known.end() ? token : std::to_string(it->second);
}

}  // namespace

// ---------------------------------------------------------------------------
// Phase 2
// ---------------------------------------------------------------------------
// Time is a straight sum. Memory is a liveness question: a computed value
// occupies a register from the instruction that defines it until the last one
// that reads it, and `res` is read by whoever runs the program, so it never
// dies. Peak pressure is then the largest overlap of those intervals, which is
// why the sweep below is a max over instructions.
Metrics analyze(const std::vector<Instr>& program, const std::map<std::string, int>& costs) {
    Metrics metrics;

    for (const auto& instr : program) {
        if (instr.has_op()) {
            metrics.time += costs.at(instr.op);  // throws std::out_of_range if undeclared
        }
    }

    std::unordered_map<std::string, long long> last_use;
    for (std::size_t i = 0; i < program.size(); ++i) {
        for (const auto& operand : program[i].operands()) {
            if (!is_literal(operand)) {
                last_use[operand] = static_cast<long long>(i);
            }
        }
    }

    std::unordered_map<std::string, long long> defined_at;
    for (std::size_t i = 0; i < program.size(); ++i) {
        defined_at.emplace(program[i].target, static_cast<long long>(i));
    }

    for (std::size_t i = 0; i < program.size(); ++i) {
        int live = 0;
        for (const auto& [name, birth] : defined_at) {
            if (birth > static_cast<long long>(i)) continue;
            const auto it = last_use.find(name);
            const bool read_later = it != last_use.end() && it->second > static_cast<long long>(i);
            if (name == kOutput || read_later) ++live;
        }
        metrics.memory = std::max(metrics.memory, live);
    }
    return metrics;
}

// ---------------------------------------------------------------------------
// Phase 3
// ---------------------------------------------------------------------------
// A worklist, not a single reverse pass: dropping one assignment can orphan the
// ones that only fed it, so the answer is the fixed point of "reachable from
// res".
std::vector<Instr> eliminate_dead_code(const std::vector<Instr>& program) {
    std::unordered_map<std::string, std::size_t> defined_at;
    for (std::size_t i = 0; i < program.size(); ++i) {
        defined_at[program[i].target] = i;
    }

    std::unordered_set<std::string> needed;
    std::vector<std::string> stack{kOutput};
    while (!stack.empty()) {
        const std::string name = stack.back();
        stack.pop_back();
        if (needed.count(name) || defined_at.count(name) == 0) {
            continue;  // already handled, or an input variable
        }
        needed.insert(name);
        for (const auto& operand : program[defined_at.at(name)].operands()) {
            if (!is_literal(operand)) {
                stack.push_back(operand);
            }
        }
    }

    std::vector<Instr> live;
    for (const auto& instr : program) {
        if (needed.count(instr.target)) {
            live.push_back(instr);
        }
    }
    return live;
}

// ---------------------------------------------------------------------------
// Phase 4
// ---------------------------------------------------------------------------
// Single assignment means a target's value never changes once computed, so a
// plain forward walk with a `known` table is enough -- no dataflow fixed point
// needed here.
std::vector<Instr> fold_constants(const std::vector<Instr>& program) {
    std::unordered_map<std::string, long long> known;
    std::vector<Instr> out;
    out.reserve(program.size());

    for (const auto& instr : program) {
        const std::string left = resolve(instr.left, known);

        if (!instr.has_op()) {
            if (is_literal(left)) {
                known[instr.target] = std::stoll(left);
            }
            out.push_back(Instr{instr.target, left, "", ""});
            continue;
        }

        const std::string right = resolve(instr.right, known);
        if (is_literal(left) && is_literal(right)) {
            const auto value = apply_op(instr.op, std::stoll(left), std::stoll(right));
            if (value.has_value()) {
                known[instr.target] = *value;
                out.push_back(Instr{instr.target, std::to_string(*value), "", ""});
                continue;
            }
            known.erase(instr.target);  // division by zero: leave it to runtime
        }
        out.push_back(Instr{instr.target, left, instr.op, right});
    }
    return out;
}

std::pair<std::vector<Instr>, Metrics> optimize(const std::vector<Instr>& program,
                                                const std::map<std::string, int>& costs) {
    const std::vector<Instr> folded = fold_constants(program);
    const std::vector<Instr> live = eliminate_dead_code(folded);
    return {live, analyze(live, costs)};
}
