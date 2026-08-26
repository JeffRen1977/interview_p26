# Compiler Optimization — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read optimizer_utils.py, compiler_optimizer.py, tests/data/case_04_mixed.txt
and tests/test_compiler_optimizer.py.
Do NOT write implementation code.

List:
1. The exact case-file format, including where comments may appear
2. The exact definition of the `memory` metric — when a computed value starts
   being live, when it stops, and what happens to `res`
3. Which variables do NOT count toward memory
4. What optimize() does and in what order, and why that order is not arbitrary
5. Which of the four phases each failing test belongs to
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Three Phase-1 failures:
  test_costs_are_keyed_by_the_bare_operator
  test_program_lines_drop_trailing_comments
  test_every_case_file_loads_and_parses

Explain each root cause in one sentence, naming the exact line in load_case.
Note that the costs branch and the expected branch of the same if/elif do not
treat their keys the same way — say which one is right.
Do NOT propose code yet.
```

期望它说出：`partition("=")` 留下 `"+ "`（costs 分支忘了 strip，expected 分支没忘）；注释过滤只挡了 `startswith("#")`，行尾注释活到了 `parse_program` 里变成 5 个 token。

---

## Prompt 2 — Phase 1 修复

```
Task: fix load_case in optimizer_utils.py.
Constraints:
- A '#' starts a comment anywhere on the line, not just in column 0; strip it
  before the blank-line check so full-line comments still get skipped
- Cost keys are the bare operator character: costs['+'], never costs['+ ']
- Do not change the [expected] branch, the section detection, or any signature
- Do not touch parse_program, is_literal or Instr
Return only the changed function.
```

跑：`python3 -m unittest discover -s tests -k Phase1 -v`

---

## Prompt 3 — Phase 2 分析器

```
Task: implement analyze(program, costs) -> Metrics in compiler_optimizer.py.

time: sum of costs[instr.op] over instructions that have an operator; a plain
copy costs 0; an operator missing from costs raises KeyError.

memory: peak number of live computed values, defined as
    max over i of |{ v : v defined at some j <= i,
                         and (v == OUTPUT or v is read at some k > i) }|
- only assignment targets count; input variables and integer literals do not
- OUTPUT ("res") is live from its definition to the end of the program
- a target that is never read and is not res contributes 0 to memory
- an empty program is Metrics(0, 0)

Constraints:
- Build a last-use table first, then sweep; use is_literal to skip literals
- Do NOT implement eliminate_dead_code or fold_constants
Return only the function body plus private helpers.
```

跑：`python3 -m unittest discover -s tests -k Phase2 -v`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste unittest output>
```

---

## Prompt 5 — Phase 3 死代码消除

```
Task: implement eliminate_dead_code(program) -> List[Instr].
Constraints:
- Backward reachability over the def-use graph, rooted at OUTPUT ("res")
- It must reach a FIXED POINT: removing an instruction can orphan the ones that
  only fed it, so a single reverse pass is not enough — use a worklist
- Literal operands and input variables keep nothing alive
- Surviving instructions keep their original relative order
- A program with no `res` returns []
- Do NOT change analyze or fold_constants
Return only the function body.
```

跑：`python3 -m unittest discover -s tests -k Phase3 -v`

---

## Prompt 6 — Phase 4 常量折叠（把除法语义写进 prompt）

```
Task: implement fold_constants(program) -> List[Instr].
Constraints:
- Forward pass with a `known: Dict[str, int]` table. Every target is assigned
  exactly once, so plain forward propagation is sound — do NOT write an
  iterative dataflow analysis
- Substitute operands that are in `known` with their literal value
- When both operands are literals, evaluate and emit `target = <value>`, and
  record the value in `known` so it propagates further
- Division is INTEGER division that TRUNCATES TOWARD ZERO like C, not Python's
  floor: -7 / 2 is -3, 7 / -2 is -3, -7 / -2 is 3.
  Use abs(a) // abs(b) with an explicit sign; do NOT use int(a / b), which
  loses precision on large integers
- Division by a literal zero is NOT folded: emit the instruction unchanged and
  drop its target from `known`
- Remove nothing and reorder nothing — that is eliminate_dead_code's job
Return only the function body plus private helpers.
```

跑：`python3 -m unittest discover -s tests -k Phase4 -v`

---

## Prompt 7 — 顺序确认（只问，不写）

```
optimize() runs fold_constants then eliminate_dead_code then analyze.
Explain in two sentences why swapping the first two would leave dead
instructions behind, using `gain = k - 2` from tests/data/case_04_mixed.txt as
the example. Then tell me whether running the pair a second time could ever
remove more.
Do NOT change code.
```

期望答案：折叠把 `gain` 的使用点改写成字面量 18，**折叠之后 `gain` 才变死**；先跑 DCE 时 `gain` 还有读者，删不掉。第二轮在这个语言子集里不会再删更多（无分支、单赋值，一轮即到不动点），但在有分支/重复赋值的真实 IR 里要迭代到不动点。

---

## Prompt 8 — 复杂度确认

```
State the complexity of analyze in terms of the number of instructions n and
the number of distinct targets m. Tell me which loop is the quadratic one and
what an O(n) formulation would look like.
Do NOT change code.
```

期望答案：last-use 表是 O(n)，扫描是 O(n·m) —— 每条指令都遍历了一遍 `defined_at`；O(n) 的写法是把每个值变成 def 处 `+1`、last_use 之后 `-1` 的差分事件，一遍前缀和取最大值。

---

## Prompt 9 — 收尾 review

```
Review compiler_optimizer.py and optimizer_utils.py against the tests only.
Call out dead code, any place the two branches of the same if/elif treat their
inputs differently, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "Two things I'd flag before this went anywhere near real IR. First, the whole
> design leans on single assignment — forward constant propagation is only sound
> because a value never changes after it's computed. Add branches or
> reassignment and this needs a real dataflow analysis with a meet operator.
> Second, `memory` here is peak *value* pressure, not register allocation: it's
> the lower bound on registers you'd need, but an allocator also has to deal
> with interference and spilling, so the real number is higher."
