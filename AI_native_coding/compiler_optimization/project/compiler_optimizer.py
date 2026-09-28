"""Cost/pressure analysis and two classic optimizations over three-address code.

Phase 2: analyze         — runtime work and peak register pressure
Phase 3: eliminate_dead_code — backward reachability from `res`
Phase 4: fold_constants  — compile-time arithmetic, integer division included
"""

from __future__ import annotations

from typing import List, Mapping, NamedTuple, Sequence, Tuple

from optimizer_utils import OUTPUT, Instr, is_literal


class Metrics(NamedTuple):
    time: int
    memory: int


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def analyze(program: Sequence[Instr], costs: Mapping[str, int]) -> Metrics:
    """Runtime work and peak live temporaries of `program`.

    time
        Sum of `costs[instr.op]` over every instruction that has an operator.
        A plain copy (`v = x`, no operator) costs nothing. An operator with no
        entry in `costs` raises KeyError.

    memory
        Peak number of *computed* values alive at the same time. Only assignment
        targets count — input variables (never assigned) and integer literals
        are free.

        A computed value is alive immediately after the instruction that defines
        it, and stays alive until the last instruction that reads it. `res` is
        the program output, so it stays alive to the end. Concretely, `memory`
        is the maximum over i of:

            |{ v : v defined at some j <= i,
                   and (v == OUTPUT or v is read at some k > i) }|

        A target that is never read and is not `res` therefore costs time but
        contributes nothing to memory.

    An empty program is Metrics(0, 0).

    TODO(Phase 2): last-use table, then sweep the instructions.
    """
    raise NotImplementedError("Phase 2: implement analyze")


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
def eliminate_dead_code(program: Sequence[Instr]) -> List[Instr]:
    """Drop every assignment whose target cannot be reached from `res`.

    Reachability runs backwards over the def-use graph and must reach a fixed
    point: removing one assignment can make its operands dead too. Literal
    operands keep nothing alive. The surviving instructions keep their original
    relative order.

    A program with no `res` at all optimizes to [].

    TODO(Phase 3): walk back from OUTPUT, collect the needed targets, filter.
    """
    raise NotImplementedError("Phase 3: implement eliminate_dead_code")


# ----------------------------------------------------------------------
# Phase 4
# ----------------------------------------------------------------------
def fold_constants(program: Sequence[Instr]) -> List[Instr]:
    """Evaluate whatever is known at compile time and propagate it forward.

    Walk the program in order, tracking which targets hold a known constant.
    Substitute those operands with their literal value. When both operands of
    an instruction are literals, evaluate it and emit `target = <value>` in its
    place, which makes the constant available to later instructions too.

    Division is INTEGER division that TRUNCATES TOWARD ZERO, like C — not
    Python's `//`, which floors. `-7 / 2` is -3, not -4.

    Division by a literal zero is not folded: emit the instruction unchanged and
    stop treating its target as known.

    Instruction order and targets are preserved; only the right-hand sides
    change. Nothing is removed here — that is what eliminate_dead_code is for.

    TODO(Phase 4): forward pass with a `known: Dict[str, int]` table.
    """
    raise NotImplementedError("Phase 4: implement fold_constants")


def optimize(program: Sequence[Instr], costs: Mapping[str, int]) -> Tuple[List[Instr], Metrics]:
    """Fold first, then eliminate dead code, then measure what is left.

    The order matters: folding turns uses of a variable into literals, which is
    exactly what makes the assignment that produced it dead.
    """
    folded = fold_constants(program)
    live = eliminate_dead_code(folded)
    return live, analyze(live, costs)
