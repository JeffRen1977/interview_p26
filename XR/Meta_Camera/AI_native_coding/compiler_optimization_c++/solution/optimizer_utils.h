// Case loading and three-address-code parsing. Reference implementation.
#pragma once

#include <map>
#include <string>
#include <vector>

//: The program's output. Everything not reachable from it is dead.
inline constexpr const char* kOutput = "res";

//: `target = left` or `target = left op right`.
//: `op` is empty for a plain copy, in which case `right` is empty too.
//: Operands are variables or integer literals; every target is assigned once.
struct Instr {
    std::string target;
    std::string left;
    std::string op;
    std::string right;

    bool has_op() const { return !op.empty(); }

    // Operands in source order, literals included.
    std::vector<std::string> operands() const {
        if (op.empty()) return {left};
        return {left, right};
    }

    bool operator==(const Instr& other) const {
        return target == other.target && left == other.left && op == other.op &&
               right == other.right;
    }
};

//: One case_*.txt fixture: the cost model, the expected optimized totals, and
//: the raw program lines.
struct Case {
    std::string name;
    std::map<std::string, int> costs;
    int expected_time = 0;
    int expected_memory = 0;
    std::vector<std::string> program;
};

// Read one case file. Throws std::invalid_argument on a malformed file.
//
// The format is three sections. Blank lines are ignored, and so is everything
// from a '#' to the end of the line -- whether the '#' starts the line or
// trails an entry.
//
//     [costs]
//     + = 1
//     * = 4
//
//     [expected]        <- the totals AFTER optimize()
//     time = 9
//     memory = 2
//
//     [program]
//     t1 = a + b        # comments may trail a program line
//     res = t1 * 2
//
// Cost keys are the bare operator characters: "+", not "+ ".
// A line that appears before any section header is an error.
Case load_case(const std::string& path);

// Every case_*.txt in `directory`, sorted by file name.
std::vector<Case> load_all_cases(const std::string& directory);

// ---------------------------------------------------------------------------
// Given and correct -- do not change these.
// ---------------------------------------------------------------------------

// True for an integer literal, false for a variable name.
bool is_literal(const std::string& token);

// Turn cleaned-up source lines into Instr objects.
// Throws std::invalid_argument if a line is not a well-formed assignment.
std::vector<Instr> parse_program(const std::vector<std::string>& lines);
