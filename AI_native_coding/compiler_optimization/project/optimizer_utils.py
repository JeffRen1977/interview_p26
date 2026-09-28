"""Case loading and three-address-code parsing.

Phase 1 lives in `load_case`: it mis-reads the case files, which makes every
later phase compare against numbers that were never right. Find it from the
failing assertions before you touch anything.

`Instr`, `parse_program` and `is_literal` below are given and correct.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional

#: The program's output. Everything not reachable from it is dead.
OUTPUT = "res"

#: The only operators a program may use.
OPERATORS = ("+", "-", "*", "/")

_LITERAL = re.compile(r"-?\d+")


@dataclass
class Case:
    """One `case_*.txt` fixture: the cost model, the expected optimized totals,
    and the raw program lines."""

    name: str
    costs: Dict[str, int] = field(default_factory=dict)
    expected_time: int = 0
    expected_memory: int = 0
    program: List[str] = field(default_factory=list)


@dataclass(frozen=True)
class Instr:
    """`target = left` or `target = left op right`. Operands are variables or
    integer literals; every target is assigned exactly once."""

    target: str
    left: str
    op: Optional[str] = None
    right: Optional[str] = None

    def operands(self) -> Iterable[str]:
        yield self.left
        if self.right is not None:
            yield self.right


def load_case(path) -> Case:
    """Read one case file.

    The format is three sections. Blank lines are ignored, and so is everything
    from a '#' to the end of the line — whether the '#' starts the line or
    trails an entry.

        [costs]
        + = 1
        * = 4

        [expected]        <- the totals AFTER optimize()
        time = 9
        memory = 2

        [program]
        t1 = a + b        # comments may trail a program line
        res = t1 * 2

    Cost keys are the bare operator characters: '+', not '+ '.
    A line that appears before any section header is an error.
    """
    path = Path(path)
    costs: Dict[str, int] = {}
    expected: Dict[str, int] = {}
    program: List[str] = []
    section: Optional[str] = None

    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip().lower()
            continue
        if section == "costs":
            key, _, value = line.partition("=")
            costs[key] = int(value)
        elif section == "expected":
            key, _, value = line.partition("=")
            expected[key.strip()] = int(value)
        elif section == "program":
            program.append(line)
        else:
            raise ValueError(f"line outside any section: {line!r}")

    missing = {"time", "memory"} - set(expected)
    if missing:
        raise ValueError(f"{path.name}: [expected] is missing {sorted(missing)}")
    return Case(
        name=path.stem,
        costs=costs,
        expected_time=expected["time"],
        expected_memory=expected["memory"],
        program=program,
    )


def load_all_cases(directory) -> List[Case]:
    """Every case_*.txt in `directory`, sorted by file name."""
    return [load_case(p) for p in sorted(Path(directory).glob("case_*.txt"))]


# ----------------------------------------------------------------------
# Given and correct — do not change the rest of this file.
# ----------------------------------------------------------------------
def is_literal(token: str) -> bool:
    """True for an integer literal, false for a variable name."""
    return bool(_LITERAL.fullmatch(token))


def parse_program(lines: Iterable[str]) -> List[Instr]:
    """Turn cleaned-up source lines into Instr objects."""
    out: List[Instr] = []
    for line in lines:
        target, sep, rhs = line.partition("=")
        if not sep:
            raise ValueError(f"not an assignment: {line!r}")
        tokens = rhs.split()
        if len(tokens) == 1:
            out.append(Instr(target.strip(), tokens[0]))
        elif len(tokens) == 3 and tokens[1] in OPERATORS:
            out.append(Instr(target.strip(), tokens[0], tokens[1], tokens[2]))
        else:
            raise ValueError(f"cannot parse instruction: {line!r}")
    return out
