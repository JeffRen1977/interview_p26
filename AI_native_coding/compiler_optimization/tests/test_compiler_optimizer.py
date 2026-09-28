"""Staged spec for the compiler-optimization task. These tests are the contract.

Run against the interview skeleton (project/, starts red):
    python3 -m unittest discover -s tests -v

Run against the reference implementation (solution/, must be green):
    AINC_IMPL=solution python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

DATA = Path(__file__).resolve().parent / "data"

from compiler_optimizer import (  # noqa: E402
    Metrics,
    analyze,
    eliminate_dead_code,
    fold_constants,
    optimize,
)
from optimizer_utils import (  # noqa: E402
    Instr,
    is_literal,
    load_all_cases,
    load_case,
    parse_program,
)

COSTS = {"+": 1, "-": 1, "*": 4, "/": 10}


def prog(*lines: str):
    return parse_program(lines)


def render(program) -> list:
    return [
        f"{i.target} = {i.left} {i.op} {i.right}" if i.op else f"{i.target} = {i.left}"
        for i in program
    ]


# ----------------------------------------------------------------------
# Phase 1 — the case loader
# ----------------------------------------------------------------------
class Phase1Loader(unittest.TestCase):
    def setUp(self) -> None:
        self.case = load_case(DATA / "case_01_basic.txt")

    def test_reads_the_expected_totals(self) -> None:
        self.assertEqual(self.case.expected_time, 6)
        self.assertEqual(self.case.expected_memory, 2)

    def test_case_name_comes_from_the_file_stem(self) -> None:
        self.assertEqual(self.case.name, "case_01_basic")

    def test_skips_blank_lines_and_full_line_comments(self) -> None:
        for line in self.case.program:
            self.assertTrue(line.strip())
            self.assertFalse(line.startswith("#"))

    def test_line_outside_a_section_is_rejected(self) -> None:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as handle:
            handle.write("t1 = a + b\n[program]\nres = t1\n")
            path = handle.name
        try:
            with self.assertRaises(ValueError):
                load_case(path)
        finally:
            os.unlink(path)

    def test_costs_are_keyed_by_the_bare_operator(self) -> None:
        """'+ = 1' declares the cost of '+', not of '+ '."""
        self.assertEqual(self.case.costs, {"+": 1, "-": 1, "*": 4, "/": 10})
        self.assertEqual(self.case.costs["*"], 4)

    def test_program_lines_drop_trailing_comments(self) -> None:
        """A '#' starts a comment wherever it appears, not just in column 0."""
        case = load_case(DATA / "case_04_mixed.txt")
        self.assertEqual(
            case.program,
            [
                "k = 4 * 5",
                "gain = k - 2",
                "noise = a * b",
                "tmp = noise + 1",
                "scaled = pixel * gain",
                "res = scaled + 1",
            ],
        )

    def test_every_case_file_loads_and_parses(self) -> None:
        """Whatever load_case returns has to survive parse_program.

        This is the test that shows the loader bug is not cosmetic: leftover
        comment text turns a 3-token right-hand side into a 5-token one.
        """
        cases = load_all_cases(DATA)
        self.assertGreaterEqual(len(cases), 4)
        for case in cases:
            self.assertTrue(case.costs, case.name)
            self.assertTrue(case.program, case.name)
            parse_program(case.program)

    def test_is_literal_tells_operands_apart(self) -> None:
        self.assertTrue(is_literal("42"))
        self.assertTrue(is_literal("-7"))
        self.assertFalse(is_literal("t1"))
        self.assertFalse(is_literal("x2"))


# ----------------------------------------------------------------------
# Phase 2 — the analyzer
# ----------------------------------------------------------------------
class Phase2Analyze(unittest.TestCase):
    def test_time_sums_the_operator_costs(self) -> None:
        program = prog("t1 = a + b", "t2 = c * d", "res = t1 - t2")
        self.assertEqual(analyze(program, COSTS).time, 1 + 4 + 1)

    def test_a_plain_copy_costs_nothing(self) -> None:
        self.assertEqual(analyze(prog("res = a"), COSTS).time, 0)
        self.assertEqual(analyze(prog("res = 7"), COSTS).time, 0)

    def test_an_operator_with_no_declared_cost_is_an_error(self) -> None:
        with self.assertRaises(KeyError):
            analyze(prog("res = a / b"), {"+": 1})

    def test_empty_program(self) -> None:
        self.assertEqual(analyze([], COSTS), Metrics(0, 0))

    def test_a_linear_chain_needs_one_register(self) -> None:
        program = prog("t1 = a + b", "t2 = t1 * c", "res = t2 - d")
        self.assertEqual(analyze(program, COSTS).memory, 1)

    def test_a_balanced_tree_needs_two(self) -> None:
        program = prog("t1 = a + b", "t2 = c + d", "res = t1 * t2")
        self.assertEqual(analyze(program, COSTS).memory, 2)

    def test_pressure_grows_with_the_number_of_pending_values(self) -> None:
        program = prog(
            "t1 = a + b",
            "t2 = c + d",
            "t3 = e + f",
            "t4 = t1 * t2",
            "res = t3 - t4",
        )
        # after t3 the live set is {t1, t2, t3}
        self.assertEqual(analyze(program, COSTS).memory, 3)

    def test_inputs_and_literals_are_free(self) -> None:
        """Only assignment targets occupy a register."""
        program = prog("res = a + 1")
        self.assertEqual(analyze(program, COSTS), Metrics(1, 1))

    def test_res_stays_live_to_the_end(self) -> None:
        program = prog("res = a + b", "unused = c + d")
        self.assertEqual(analyze(program, COSTS).memory, 1)

    def test_a_dead_assignment_costs_time_but_not_memory(self) -> None:
        program = prog("t1 = a + b", "junk = c * d", "res = t1 - e")
        metrics = analyze(program, COSTS)
        self.assertEqual(metrics.time, 1 + 4 + 1)
        self.assertEqual(metrics.memory, 1)

    def test_matches_the_expected_totals_when_nothing_can_be_optimized(self) -> None:
        case = load_case(DATA / "case_01_basic.txt")
        metrics = analyze(parse_program(case.program), case.costs)
        self.assertEqual(metrics, Metrics(case.expected_time, case.expected_memory))


# ----------------------------------------------------------------------
# Phase 3 — dead code elimination
# ----------------------------------------------------------------------
class Phase3DeadCode(unittest.TestCase):
    def test_removes_an_unused_assignment(self) -> None:
        program = prog("t1 = a + b", "junk = c * d", "res = t1 - e")
        self.assertEqual(render(eliminate_dead_code(program)), ["t1 = a + b", "res = t1 - e"])

    def test_removal_cascades_backwards(self) -> None:
        """Dropping `junk` orphans `t2`, whose only reader was `junk`."""
        program = prog("t1 = a + b", "t2 = c + d", "junk = t1 * t2", "res = t1 - e")
        self.assertEqual(render(eliminate_dead_code(program)), ["t1 = a + b", "res = t1 - e"])

    def test_keeps_everything_reachable_from_res(self) -> None:
        program = prog("t1 = a + b", "t2 = t1 * c", "res = t2 - d")
        self.assertEqual(eliminate_dead_code(program), list(program))

    def test_preserves_the_original_order(self) -> None:
        program = prog("t1 = a + b", "t2 = c + d", "res = t2 - t1")
        self.assertEqual(render(eliminate_dead_code(program)), render(program))

    def test_literal_operands_keep_nothing_alive(self) -> None:
        program = prog("t1 = a + b", "res = 3 * 4")
        self.assertEqual(render(eliminate_dead_code(program)), ["res = 3 * 4"])

    def test_a_program_without_res_optimizes_to_nothing(self) -> None:
        self.assertEqual(eliminate_dead_code(prog("t1 = a + b", "t2 = t1 * c")), [])

    def test_eliminating_dead_code_lowers_the_reported_time(self) -> None:
        case = load_case(DATA / "case_02_dead_code.txt")
        program = parse_program(case.program)
        before = analyze(program, case.costs)
        after = analyze(eliminate_dead_code(program), case.costs)
        self.assertLess(after.time, before.time)
        self.assertEqual(after, Metrics(case.expected_time, case.expected_memory))


# ----------------------------------------------------------------------
# Phase 4 — constant folding
# ----------------------------------------------------------------------
class Phase4ConstantFolding(unittest.TestCase):
    def test_folds_a_literal_pair(self) -> None:
        self.assertEqual(render(fold_constants(prog("res = 6 * 7"))), ["res = 42"])

    def test_folding_propagates_through_variables(self) -> None:
        program = prog("two = 2", "six = two * 3", "res = six + 1")
        self.assertEqual(render(fold_constants(program)), ["two = 2", "six = 6", "res = 7"])

    def test_integer_division_truncates_toward_zero(self) -> None:
        """C semantics, not Python's floor: -7 / 2 is -3, and 7 / -2 is -3."""
        self.assertEqual(render(fold_constants(prog("res = -7 / 2"))), ["res = -3"])
        self.assertEqual(render(fold_constants(prog("res = 7 / -2"))), ["res = -3"])
        self.assertEqual(render(fold_constants(prog("res = -7 / -2"))), ["res = 3"])
        self.assertEqual(render(fold_constants(prog("res = 7 / 2"))), ["res = 3"])

    def test_division_by_zero_is_left_for_runtime(self) -> None:
        program = prog("z = 0", "bad = 5 / z", "res = bad + 1")
        folded = render(fold_constants(program))
        self.assertEqual(folded[0], "z = 0")
        self.assertEqual(folded[1], "bad = 5 / 0")
        self.assertEqual(folded[2], "res = bad + 1")

    def test_unknown_operands_survive_untouched(self) -> None:
        program = prog("k = 4 * 5", "res = pixel * k")
        self.assertEqual(render(fold_constants(program)), ["k = 20", "res = pixel * 20"])

    def test_folding_alone_removes_nothing(self) -> None:
        program = prog("k = 4 * 5", "res = pixel * k")
        self.assertEqual(len(fold_constants(program)), len(program))

    def test_fold_then_dce_drops_the_folded_copies(self) -> None:
        program = prog("k = 4 * 5", "gain = k - 2", "res = pixel * gain")
        live, metrics = optimize(program, COSTS)
        self.assertEqual(render(live), ["res = pixel * 18"])
        self.assertEqual(metrics, Metrics(4, 1))

    def test_every_case_file_hits_its_expected_totals(self) -> None:
        for case in load_all_cases(DATA):
            _, metrics = optimize(parse_program(case.program), case.costs)
            self.assertEqual(
                metrics,
                Metrics(case.expected_time, case.expected_memory),
                f"{case.name}: optimized totals do not match [expected]",
            )

    def test_optimizing_never_makes_a_program_worse(self) -> None:
        for case in load_all_cases(DATA):
            program = parse_program(case.program)
            before = analyze(program, case.costs)
            _, after = optimize(program, case.costs)
            self.assertLessEqual(after.time, before.time, case.name)
            self.assertLessEqual(after.memory, before.memory, case.name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
