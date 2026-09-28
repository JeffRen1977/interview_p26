# Compiler Optimization (C++) — 逐阶段 Prompt

> 结构与 [`../compiler_optimization/prompts.md`](../compiler_optimization/prompts.md) 一致。
> **C++ 特有的约束都写进 prompt 了** —— 尤其是 Phase 4 的整数宽度和 UB，AI 默认不会替你想。

---

## Prompt 0 — 抽 spec（不写代码）

```
Read optimizer_utils.h/.cpp, compiler_optimizer.h/.cpp,
tests/data/case_04_mixed.txt and tests/test_compiler_optimizer.cpp.
Do NOT write implementation code.

List:
1. The exact case-file format, including where comments may appear
2. The exact definition of the `memory` metric -- when a computed value starts
   being live, when it stops, and what happens to `res`
3. Which variables do NOT count toward memory
4. Which exception type each failure mode uses (missing operator cost,
   malformed file, unimplemented stub) and how they relate by inheritance
5. What optimize() does and in what order, and why that order is not arbitrary
```

期望它指出 `std::out_of_range` 和 `std::invalid_argument` 都继承自 `std::logic_error` —— 这决定了测试 runner 怎么归类失败。

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Three Phase-1 failures:
  test_costs_are_keyed_by_the_bare_operator
    -> cost table keys are wrong: {"* ":4 "+ ":1 "- ":1 "/ ":10 }
  test_program_lines_drop_trailing_comments
  test_every_case_file_loads_and_parses

Explain each root cause in one sentence, naming the exact line in load_case.
Note that the costs branch and the expected branch of the same if/else if do
not treat their keys the same way -- say which one is right.
Also explain why std::stoi did NOT mask the first bug.
Do NOT propose code yet.
```

期望它说出：`substr(0, pos)` 留下 `"+ "`（costs 分支忘了 `trim`，expected 分支没忘）；`std::stoi` 跳过前导空白所以 value 侧无恙；注释过滤只挡了 `line[0] == '#'`，行尾注释活到了 `parse_program` 里变成 5 个 token。

---

## Prompt 2 — Phase 1 修复

```
Task: fix load_case in optimizer_utils.cpp.
Constraints:
- A '#' starts a comment anywhere on the line, not just in column 0. Strip it
  from the RAW line before trimming and before the empty check, so a full-line
  comment simply becomes empty and hits the existing guard
- Cost keys are the bare operator character: costs["+"], never costs["+ "]
- Do not change the [expected] branch, the section detection, or any signature
- Do not touch parse_program, is_literal or Instr
Return only the changed function.
```

跑：`make test`（只看 Phase1Loader 那一段）

---

## Prompt 3 — Phase 2 分析器

```
Task: implement analyze(program, costs) -> Metrics in compiler_optimizer.cpp.

time: sum of costs.at(instr.op) over instructions with an operator; a plain
copy costs 0; costs.at throws std::out_of_range for an undeclared operator,
which is the contract -- do not catch it.

memory: peak number of live computed values, defined as
    max over i of |{ v : v defined at some j <= i,
                         and (v == kOutput or v is read at some k > i) }|
- only assignment targets count; input variables and integer literals do not
- kOutput ("res") is live from its definition to the end of the program
- a target that is never read and is not res contributes 0 to memory
- an empty program is Metrics{0, 0}

Constraints:
- Build a last-use table first, then sweep; use is_literal to skip literals
- Store indices as signed (long long), NOT size_t: the "never read" sentinel is
  compared against -1 and unsigned would wrap
- Use defined_at.emplace, not operator[], so the FIRST definition wins
- Metrics::time is long long; do not narrow it
- Do NOT implement eliminate_dead_code or fold_constants
Return only the function body plus anonymous-namespace helpers.
```

跑：`make test`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste the failing test name and its message>
```

---

## Prompt 5 — Phase 3 死代码消除

```
Task: implement eliminate_dead_code(program) -> std::vector<Instr>.
Constraints:
- Backward reachability over the def-use graph, rooted at kOutput ("res")
- It must reach a FIXED POINT: removing an instruction can orphan the ones that
  only fed it, so a single reverse pass is not enough -- use a worklist
- The worklist is a std::vector<std::string>. Take the top BY VALUE before
  pop_back(): a reference into the vector dangles the moment you pop or push
- Literal operands and input variables keep nothing alive
- Surviving instructions keep their original relative order
- A program with no `res` returns {}
- Do NOT change analyze or fold_constants
Return only the function body.
```

跑：`make test`

---

## Prompt 6 — Phase 4 常量折叠（整数宽度写进 prompt）

```
Task: implement fold_constants(program) -> std::vector<Instr>.
Constraints:
- Forward pass with std::unordered_map<std::string, long long> known. Every
  target is assigned exactly once, so plain forward propagation is sound --
  do NOT write an iterative dataflow analysis
- Substitute operands present in `known` with std::to_string of their value
- When both operands are literals, evaluate and emit `target = <value>`, and
  record it in `known` so it propagates further
- Parse literals with std::stoll and compute in long long. std::stoi would
  throw on 4000000000, and int arithmetic would be signed overflow (UB)
- Integer division in C++ already truncates toward zero -- just use a / b.
  Do NOT route it through double or std::floor
- Return std::nullopt (do not fold) for b == 0, and also for
  a == std::numeric_limits<long long>::min() && b == -1, which overflows
- On a non-folded division, erase the target from `known`
- Remove nothing and reorder nothing -- that is eliminate_dead_code's job
Return only the function body plus anonymous-namespace helpers.
```

跑：`make test IMPL=project` 然后对照 `make test IMPL=solution`

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

## Prompt 8 — UB 审查（C++ 版专属，这条最值钱）

```
Review compiler_optimizer.cpp for undefined behaviour and integer-width bugs
only. Specifically check:
- any reference or iterator held across a container mutation
- signed overflow in the folding arithmetic
- unsigned wraparound where a "not found" sentinel is compared
- std::stoi vs std::stoll on the literals in tests/data
Report findings with file:line. Do NOT change code yet.
```

然后自己跑一遍 sanitizer 验证它说得对不对：

```bash
make clean
make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"
```

**面试里主动跑这一条**，比嘴上说"我会注意 UB"强十倍。

---

## Prompt 9 — 复杂度确认

```
State the complexity of analyze in terms of the number of instructions n and
the number of distinct targets m. Tell me which loop is the quadratic one and
what an O(n) formulation would look like.
Do NOT change code.
```

期望答案：last-use 表是 O(n)，扫描是 O(n·m) —— 每条指令都遍历了一遍 `defined_at`；O(n) 的写法是把每个值变成 def 处 `+1`、last_use 之后 `-1` 的差分事件，一遍前缀和取最大值。

---

## Prompt 10 — 收尾 review

```
Review compiler_optimizer.cpp and optimizer_utils.cpp against the tests only.
Call out dead code, any place the two branches of the same if/else if treat
their inputs differently, and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己主动说一句（面试官在等这句）：

> "Three things I'd flag before this went near real IR. First, the whole design
> leans on single assignment — forward constant propagation is only sound
> because a value never changes after it's computed. Add branches or
> reassignment and this needs a real dataflow analysis with a meet operator.
> Second, `memory` here is peak *value* pressure, not register allocation: it's
> a lower bound on the registers you'd need, but an allocator also has to handle
> interference and spilling, so the real number is higher. Third, folding into
> `long long` matches this test set but a real compiler folds in the *target*
> type — if the source language says 32-bit int, folding `2000000000 +
> 2000000000` to 4000000000 is wrong; you have to reproduce the target's
> wraparound or refuse to fold."
