"""Cost/pressure analysis and two classic optimizations. Reference implementation."""

from __future__ import annotations

from typing import Dict, List, Mapping, NamedTuple, Optional, Sequence, Tuple

from optimizer_utils import OUTPUT, Instr, is_literal


class Metrics(NamedTuple):
    time: int
    memory: int


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def analyze(program: Sequence[Instr], costs: Mapping[str, int]) -> Metrics:
    """Runtime work plus peak register pressure.

    Time is a straight sum. Memory is a liveness question: a computed value
    occupies a register from the instruction that defines it until the last one
    that reads it, and `res` is read by whoever runs the program, so it never
    dies. Half-open intervals — [def, last_use] — mean the peak is just the
    largest overlap, which is why the sweep below is a max over instructions.
    """
    total = 0
    for instr in program:
        if instr.op is not None:
            if instr.op not in costs:
                raise KeyError(f"no cost declared for operator {instr.op!r}")
            total += costs[instr.op]

    last_use: Dict[str, int] = {}
    for i, instr in enumerate(program):
        for operand in instr.operands():
            if not is_literal(operand):
                last_use[operand] = i

    defined_at: Dict[str, int] = {}
    for i, instr in enumerate(program):
        defined_at.setdefault(instr.target, i)

    peak = 0
    for i in range(len(program)):
        live = 0
        for name, birth in defined_at.items():
            if birth > i:
                continue
            if name == OUTPUT or last_use.get(name, -1) > i:
                live += 1
        peak = max(peak, live)
    return Metrics(total, peak)


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
def eliminate_dead_code(program: Sequence[Instr]) -> List[Instr]:
    """Backward reachability from `res` over the def-use graph.

    A worklist, not a single reverse pass: dropping one assignment can orphan
    the ones that only fed it, so the answer is the fixed point of "reachable
    from res".
    """
    defined_at: Dict[str, int] = {}
    for i, instr in enumerate(program):
        defined_at[instr.target] = i

    needed: set = set()
    stack: List[str] = [OUTPUT]
    while stack:
        name = stack.pop()
        if name in needed or name not in defined_at:
            continue  # already handled, or an input variable
        needed.add(name)
        for operand in program[defined_at[name]].operands():
            if not is_literal(operand):
                stack.append(operand)

    return [instr for instr in program if instr.target in needed]


# ----------------------------------------------------------------------
# Phase 4
# ----------------------------------------------------------------------
def fold_constants(program: Sequence[Instr]) -> List[Instr]:
    """Forward constant propagation plus folding.

    Single assignment means a target's value never changes once computed, so a
    plain forward walk with a `known` table is enough — no dataflow fixed point
    needed here.
    """
    known: Dict[str, int] = {}
    out: List[Instr] = []

    for instr in program:
        left = _resolve(instr.left, known)
        right = _resolve(instr.right, known) if instr.right is not None else None

        if instr.op is None:
            if is_literal(left):
                known[instr.target] = int(left)
            out.append(Instr(instr.target, left))
            continue

        if is_literal(left) and right is not None and is_literal(right):
            value = _apply(instr.op, int(left), int(right))
            if value is not None:
                known[instr.target] = value
                out.append(Instr(instr.target, str(value)))
                continue
            known.pop(instr.target, None)  # division by zero: leave it to runtime

        out.append(Instr(instr.target, left, instr.op, right))
    return out


def _resolve(token: str, known: Mapping[str, int]) -> str:
    return str(known[token]) if token in known else token


def _apply(op: str, a: int, b: int) -> Optional[int]:
    if op == "+":
        return a + b
    if op == "-":
        return a - b
    if op == "*":
        return a * b
    if op == "/":
        if b == 0:
            return None
        # Truncate toward zero like C. Python's // floors, so -7 // 2 == -4
        # where the language being compiled says -3.
        magnitude = abs(a) // abs(b)
        return -magnitude if (a < 0) != (b < 0) else magnitude
    raise ValueError(f"unknown operator {op!r}")


def optimize(program: Sequence[Instr], costs: Mapping[str, int]) -> Tuple[List[Instr], Metrics]:
    """Fold first, then eliminate dead code, then measure what is left."""
    folded = fold_constants(program)
    live = eliminate_dead_code(folded)
    return live, analyze(live, costs)
