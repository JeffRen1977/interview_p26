// Staged spec for the compiler-optimization task. These tests are the contract.
//
// Run against the interview skeleton (project/, starts red):
//     make test
//
// Run against the reference implementation (solution/, must be green):
//     make test IMPL=solution

#include "compiler_optimizer.h"
#include "optimizer_utils.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string kData = "tests/data";
const std::map<std::string, int> kCosts = {{"+", 1}, {"-", 1}, {"*", 4}, {"/", 10}};

int g_ok = 0;
int g_fail = 0;
int g_error = 0;

struct AssertFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void fail(const std::string& msg) { throw AssertFail(msg); }

void expect(bool cond, const std::string& msg) {
    if (!cond) fail(msg);
}

void expect_eq_int(long long got, long long want, const std::string& msg) {
    if (got != want) {
        fail(msg + " got=" + std::to_string(got) + " want=" + std::to_string(want));
    }
}

std::string show(const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ", ";
        out += "\"" + items[i] + "\"";
    }
    return out + "]";
}

void expect_eq_vec(const std::vector<std::string>& got,
                   const std::vector<std::string>& want,
                   const std::string& msg) {
    if (got != want) fail(msg + "\n  got=" + show(got) + "\n  want=" + show(want));
}

void expect_eq_metrics(const Metrics& got, const Metrics& want, const std::string& msg) {
    if (got != want) {
        fail(msg + " got=(time=" + std::to_string(got.time) + ", memory=" +
             std::to_string(got.memory) + ") want=(time=" + std::to_string(want.time) +
             ", memory=" + std::to_string(want.memory) + ")");
    }
}

// --- helpers ---------------------------------------------------------------

std::vector<Instr> prog(const std::vector<std::string>& lines) { return parse_program(lines); }

std::vector<std::string> render(const std::vector<Instr>& program) {
    std::vector<std::string> out;
    out.reserve(program.size());
    for (const auto& i : program) {
        out.push_back(i.has_op() ? i.target + " = " + i.left + " " + i.op + " " + i.right
                                 : i.target + " = " + i.left);
    }
    return out;
}

Case basic_case() { return load_case(kData + "/case_01_basic.txt"); }

// ---------------------------------------------------------------------------
// Phase 1 -- the case loader
// ---------------------------------------------------------------------------
void test_reads_the_expected_totals() {
    const Case c = basic_case();
    expect_eq_int(c.expected_time, 6, "expected_time");
    expect_eq_int(c.expected_memory, 2, "expected_memory");
}

void test_case_name_comes_from_the_file_stem() {
    expect(basic_case().name == "case_01_basic", "name should be the file stem");
}

void test_skips_blank_lines_and_full_line_comments() {
    for (const auto& line : basic_case().program) {
        expect(!line.empty(), "a blank line leaked into the program");
        expect(line[0] != '#', "a full-line comment leaked into the program: " + line);
    }
}

void test_line_outside_a_section_is_rejected() {
    const std::string path = "build/tmp_bad_case.txt";
    {
        std::ofstream out(path);
        out << "t1 = a + b\n[program]\nres = t1\n";
    }
    bool threw = false;
    try {
        load_case(path);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    std::remove(path.c_str());
    expect(threw, "a line before any section header must throw std::invalid_argument");
}

void test_costs_are_keyed_by_the_bare_operator() {
    // "+ = 1" declares the cost of "+", not of "+ ".
    const Case c = basic_case();
    const std::map<std::string, int> want = {{"+", 1}, {"-", 1}, {"*", 4}, {"/", 10}};
    if (c.costs != want) {
        std::string got = "{";
        for (const auto& [key, value] : c.costs) {
            got += "\"" + key + "\":" + std::to_string(value) + " ";
        }
        fail("cost table keys are wrong: " + got + "}");
    }
    expect_eq_int(c.costs.at("*"), 4, "costs[\"*\"]");
}

void test_program_lines_drop_trailing_comments() {
    // A '#' starts a comment wherever it appears, not just in column 0.
    const Case c = load_case(kData + "/case_04_mixed.txt");
    expect_eq_vec(c.program,
                  {"k = 4 * 5", "gain = k - 2", "noise = a * b", "tmp = noise + 1",
                   "scaled = pixel * gain", "res = scaled + 1"},
                  "program lines still carry their trailing comments");
}

void test_every_case_file_loads_and_parses() {
    // Whatever load_case returns has to survive parse_program. This is the test
    // that shows the loader bug is not cosmetic: leftover comment text turns a
    // 3-token right-hand side into a 5-token one.
    const std::vector<Case> cases = load_all_cases(kData);
    expect(cases.size() >= 4, "expected at least four case files");
    for (const auto& c : cases) {
        expect(!c.costs.empty(), c.name + ": empty cost table");
        expect(!c.program.empty(), c.name + ": empty program");
        parse_program(c.program);
    }
}

void test_is_literal_tells_operands_apart() {
    expect(is_literal("42"), "42 is a literal");
    expect(is_literal("-7"), "-7 is a literal");
    expect(!is_literal("t1"), "t1 is a variable");
    expect(!is_literal("x2"), "x2 is a variable");
    expect(!is_literal("-"), "a bare minus is not a literal");
}

// ---------------------------------------------------------------------------
// Phase 2 -- the analyzer
// ---------------------------------------------------------------------------
void test_time_sums_the_operator_costs() {
    const auto p = prog({"t1 = a + b", "t2 = c * d", "res = t1 - t2"});
    expect_eq_int(analyze(p, kCosts).time, 1 + 4 + 1, "time");
}

void test_a_plain_copy_costs_nothing() {
    expect_eq_int(analyze(prog({"res = a"}), kCosts).time, 0, "copy of a variable");
    expect_eq_int(analyze(prog({"res = 7"}), kCosts).time, 0, "copy of a literal");
}

void test_an_operator_with_no_declared_cost_is_an_error() {
    bool threw = false;
    try {
        analyze(prog({"res = a / b"}), {{"+", 1}});
    } catch (const std::out_of_range&) {
        threw = true;
    }
    expect(threw, "an undeclared operator must throw std::out_of_range");
}

void test_empty_program() { expect_eq_metrics(analyze({}, kCosts), Metrics{0, 0}, "empty"); }

void test_a_linear_chain_needs_one_register() {
    const auto p = prog({"t1 = a + b", "t2 = t1 * c", "res = t2 - d"});
    expect_eq_int(analyze(p, kCosts).memory, 1, "accumulator chain");
}

void test_a_balanced_tree_needs_two() {
    const auto p = prog({"t1 = a + b", "t2 = c + d", "res = t1 * t2"});
    expect_eq_int(analyze(p, kCosts).memory, 2, "balanced tree");
}

void test_pressure_grows_with_the_number_of_pending_values() {
    const auto p = prog({"t1 = a + b", "t2 = c + d", "t3 = e + f", "t4 = t1 * t2", "res = t3 - t4"});
    // after t3 the live set is {t1, t2, t3}
    expect_eq_int(analyze(p, kCosts).memory, 3, "three pending values");
}

void test_inputs_and_literals_are_free() {
    // Only assignment targets occupy a register.
    expect_eq_metrics(analyze(prog({"res = a + 1"}), kCosts), Metrics{1, 1}, "res = a + 1");
}

void test_res_stays_live_to_the_end() {
    const auto p = prog({"res = a + b", "unused = c + d"});
    expect_eq_int(analyze(p, kCosts).memory, 1, "res outlives the program");
}

void test_a_dead_assignment_costs_time_but_not_memory() {
    const auto p = prog({"t1 = a + b", "junk = c * d", "res = t1 - e"});
    const Metrics m = analyze(p, kCosts);
    expect_eq_int(m.time, 1 + 4 + 1, "dead code still costs time");
    expect_eq_int(m.memory, 1, "dead code occupies no register");
}

void test_matches_the_expected_totals_when_nothing_can_be_optimized() {
    const Case c = basic_case();
    expect_eq_metrics(analyze(parse_program(c.program), c.costs),
                      Metrics{c.expected_time, c.expected_memory}, c.name);
}

// ---------------------------------------------------------------------------
// Phase 3 -- dead code elimination
// ---------------------------------------------------------------------------
void test_removes_an_unused_assignment() {
    const auto p = prog({"t1 = a + b", "junk = c * d", "res = t1 - e"});
    expect_eq_vec(render(eliminate_dead_code(p)), {"t1 = a + b", "res = t1 - e"}, "junk survived");
}

void test_removal_cascades_backwards() {
    // Dropping `junk` orphans `t2`, whose only reader was `junk`.
    const auto p = prog({"t1 = a + b", "t2 = c + d", "junk = t1 * t2", "res = t1 - e"});
    expect_eq_vec(render(eliminate_dead_code(p)), {"t1 = a + b", "res = t1 - e"},
                  "removal did not cascade");
}

void test_keeps_everything_reachable_from_res() {
    const auto p = prog({"t1 = a + b", "t2 = t1 * c", "res = t2 - d"});
    expect(eliminate_dead_code(p) == p, "nothing here is dead");
}

void test_preserves_the_original_order() {
    const auto p = prog({"t1 = a + b", "t2 = c + d", "res = t2 - t1"});
    expect_eq_vec(render(eliminate_dead_code(p)), render(p), "order changed");
}

void test_literal_operands_keep_nothing_alive() {
    const auto p = prog({"t1 = a + b", "res = 3 * 4"});
    expect_eq_vec(render(eliminate_dead_code(p)), {"res = 3 * 4"}, "t1 survived");
}

void test_a_program_without_res_optimizes_to_nothing() {
    expect(eliminate_dead_code(prog({"t1 = a + b", "t2 = t1 * c"})).empty(),
           "with no res, everything is dead");
}

void test_eliminating_dead_code_lowers_the_reported_time() {
    const Case c = load_case(kData + "/case_02_dead_code.txt");
    const auto p = parse_program(c.program);
    const Metrics before = analyze(p, c.costs);
    const Metrics after = analyze(eliminate_dead_code(p), c.costs);
    expect(after.time < before.time, "dce should have removed work");
    expect_eq_metrics(after, Metrics{c.expected_time, c.expected_memory}, c.name);
}

// ---------------------------------------------------------------------------
// Phase 4 -- constant folding
// ---------------------------------------------------------------------------
void test_folds_a_literal_pair() {
    expect_eq_vec(render(fold_constants(prog({"res = 6 * 7"}))), {"res = 42"}, "6 * 7");
}

void test_folding_propagates_through_variables() {
    const auto p = prog({"two = 2", "six = two * 3", "res = six + 1"});
    expect_eq_vec(render(fold_constants(p)), {"two = 2", "six = 6", "res = 7"}, "propagation");
}

void test_integer_division_truncates_toward_zero() {
    // C++ integer division already truncates toward zero -- do not route this
    // through doubles or std::floor, which would give -4 for the first case.
    expect_eq_vec(render(fold_constants(prog({"res = -7 / 2"}))), {"res = -3"}, "-7 / 2");
    expect_eq_vec(render(fold_constants(prog({"res = 7 / -2"}))), {"res = -3"}, "7 / -2");
    expect_eq_vec(render(fold_constants(prog({"res = -7 / -2"}))), {"res = 3"}, "-7 / -2");
    expect_eq_vec(render(fold_constants(prog({"res = 7 / 2"}))), {"res = 3"}, "7 / 2");
}

void test_folding_uses_64_bit_arithmetic() {
    // 2e9 + 2e9 overflows a 32-bit int. Parse with stoll and fold into long long.
    expect_eq_vec(render(fold_constants(prog({"res = 2000000000 + 2000000000"}))),
                  {"res = 4000000000"}, "32-bit overflow");
    expect_eq_vec(render(fold_constants(prog({"res = 3000000 * 3000000"}))),
                  {"res = 9000000000000"}, "32-bit overflow");
}

void test_division_by_zero_is_left_for_runtime() {
    const auto folded = render(fold_constants(prog({"z = 0", "bad = 5 / z", "res = bad + 1"})));
    expect_eq_vec(folded, {"z = 0", "bad = 5 / 0", "res = bad + 1"}, "division by zero");
}

void test_unknown_operands_survive_untouched() {
    const auto p = prog({"k = 4 * 5", "res = pixel * k"});
    expect_eq_vec(render(fold_constants(p)), {"k = 20", "res = pixel * 20"}, "substitution");
}

void test_folding_alone_removes_nothing() {
    const auto p = prog({"k = 4 * 5", "res = pixel * k"});
    expect_eq_int(static_cast<long long>(fold_constants(p).size()),
                  static_cast<long long>(p.size()), "folding must not delete instructions");
}

void test_fold_then_dce_drops_the_folded_copies() {
    const auto p = prog({"k = 4 * 5", "gain = k - 2", "res = pixel * gain"});
    const auto [live, metrics] = optimize(p, kCosts);
    expect_eq_vec(render(live), {"res = pixel * 18"}, "leftover copies");
    expect_eq_metrics(metrics, Metrics{4, 1}, "optimized metrics");
}

void test_every_case_file_hits_its_expected_totals() {
    for (const auto& c : load_all_cases(kData)) {
        const auto [live, metrics] = optimize(parse_program(c.program), c.costs);
        (void)live;
        expect_eq_metrics(metrics, Metrics{c.expected_time, c.expected_memory},
                          c.name + ": optimized totals do not match [expected]");
    }
}

void test_optimizing_never_makes_a_program_worse() {
    for (const auto& c : load_all_cases(kData)) {
        const auto p = parse_program(c.program);
        const Metrics before = analyze(p, c.costs);
        const auto [live, after] = optimize(p, c.costs);
        (void)live;
        expect(after.time <= before.time, c.name + ": time got worse");
        expect(after.memory <= before.memory, c.name + ": memory got worse");
    }
}

void run(const std::string& name, void (*fn)()) {
    try {
        fn();
        ++g_ok;
        std::cout << name << " ... ok\n";
    } catch (const AssertFail& e) {
        ++g_fail;
        std::cout << name << " ... FAIL\n  " << e.what() << "\n";
    } catch (const std::logic_error& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    } catch (const std::exception& e) {
        ++g_error;
        std::cout << name << " ... ERROR\n  " << e.what() << "\n";
    }
}

}  // namespace

int main() {
    std::cout << "Phase1Loader\n";
    run("test_reads_the_expected_totals", test_reads_the_expected_totals);
    run("test_case_name_comes_from_the_file_stem", test_case_name_comes_from_the_file_stem);
    run("test_skips_blank_lines_and_full_line_comments", test_skips_blank_lines_and_full_line_comments);
    run("test_line_outside_a_section_is_rejected", test_line_outside_a_section_is_rejected);
    run("test_costs_are_keyed_by_the_bare_operator", test_costs_are_keyed_by_the_bare_operator);
    run("test_program_lines_drop_trailing_comments", test_program_lines_drop_trailing_comments);
    run("test_every_case_file_loads_and_parses", test_every_case_file_loads_and_parses);
    run("test_is_literal_tells_operands_apart", test_is_literal_tells_operands_apart);

    std::cout << "\nPhase2Analyze\n";
    run("test_time_sums_the_operator_costs", test_time_sums_the_operator_costs);
    run("test_a_plain_copy_costs_nothing", test_a_plain_copy_costs_nothing);
    run("test_an_operator_with_no_declared_cost_is_an_error", test_an_operator_with_no_declared_cost_is_an_error);
    run("test_empty_program", test_empty_program);
    run("test_a_linear_chain_needs_one_register", test_a_linear_chain_needs_one_register);
    run("test_a_balanced_tree_needs_two", test_a_balanced_tree_needs_two);
    run("test_pressure_grows_with_the_number_of_pending_values", test_pressure_grows_with_the_number_of_pending_values);
    run("test_inputs_and_literals_are_free", test_inputs_and_literals_are_free);
    run("test_res_stays_live_to_the_end", test_res_stays_live_to_the_end);
    run("test_a_dead_assignment_costs_time_but_not_memory", test_a_dead_assignment_costs_time_but_not_memory);
    run("test_matches_the_expected_totals_when_nothing_can_be_optimized", test_matches_the_expected_totals_when_nothing_can_be_optimized);

    std::cout << "\nPhase3DeadCode\n";
    run("test_removes_an_unused_assignment", test_removes_an_unused_assignment);
    run("test_removal_cascades_backwards", test_removal_cascades_backwards);
    run("test_keeps_everything_reachable_from_res", test_keeps_everything_reachable_from_res);
    run("test_preserves_the_original_order", test_preserves_the_original_order);
    run("test_literal_operands_keep_nothing_alive", test_literal_operands_keep_nothing_alive);
    run("test_a_program_without_res_optimizes_to_nothing", test_a_program_without_res_optimizes_to_nothing);
    run("test_eliminating_dead_code_lowers_the_reported_time", test_eliminating_dead_code_lowers_the_reported_time);

    std::cout << "\nPhase4ConstantFolding\n";
    run("test_folds_a_literal_pair", test_folds_a_literal_pair);
    run("test_folding_propagates_through_variables", test_folding_propagates_through_variables);
    run("test_integer_division_truncates_toward_zero", test_integer_division_truncates_toward_zero);
    run("test_folding_uses_64_bit_arithmetic", test_folding_uses_64_bit_arithmetic);
    run("test_division_by_zero_is_left_for_runtime", test_division_by_zero_is_left_for_runtime);
    run("test_unknown_operands_survive_untouched", test_unknown_operands_survive_untouched);
    run("test_folding_alone_removes_nothing", test_folding_alone_removes_nothing);
    run("test_fold_then_dce_drops_the_folded_copies", test_fold_then_dce_drops_the_folded_copies);
    run("test_every_case_file_hits_its_expected_totals", test_every_case_file_hits_its_expected_totals);
    run("test_optimizing_never_makes_a_program_worse", test_optimizing_never_makes_a_program_worse);

    const int total = g_ok + g_fail + g_error;
    std::cout << "\n----------------------------------------------------------------------\n";
    std::cout << "Ran " << total << " tests: " << g_ok << " ok, " << g_fail << " FAIL, " << g_error
              << " ERROR\n";
    return (g_fail || g_error) ? 1 : 0;
}
