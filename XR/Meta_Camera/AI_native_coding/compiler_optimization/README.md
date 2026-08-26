# Compiler Optimization — 三地址码的代价分析与优化

> Meta AI-Enabled Coding 轮换池里**最难的一道**（官方标 Hard，4 个 Checkpoint）。`project/` 是**面试开局状态**。

```bash
cd compiler_optimization
python3 -m unittest discover -s tests -v                       # 测 project/ → 2 FAIL + 28 ERROR / 35
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 35 OK
python3 -m unittest discover -s tests -k Phase2 -v             # 只跑当前 Phase
```

---

## 1. 题面与契约

给一串三地址码赋值，算两个指标，然后做两种经典优化。**不用写编译器前端** —— 词法/语法都给好了。

```
project/
├── optimizer_utils.py     # load_case              ← Phase 1 bug ×2
│                          # Instr / parse_program / is_literal（已给且正确）
└── compiler_optimizer.py  # analyze                （Phase 2 桩）
                           # eliminate_dead_code    （Phase 3 桩）
                           # fold_constants         （Phase 4 桩）
                           # optimize               （已给：fold → dce → analyze）
tests/
├── data/case_*.txt        # 四个用例：cost 模型 + 期望的优化后总量 + 程序
└── test_compiler_optimizer.py
    # Phase1Loader / Phase2Analyze / Phase3DeadCode / Phase4ConstantFolding
```

**程序模型**：每条指令是 `target = left` 或 `target = left op right`；操作数是变量或整数字面量；算子只有 `+ - * /`；**每个变量只被赋值一次**（SSA）；程序输出固定是 `res`。从不被赋值、只作为操作数出现的变量是**输入变量**。

**用例文件格式**（`#` 到行尾是注释，**行首和行尾都算**）：

```
[costs]
+ = 1
* = 4

[expected]        <- optimize() 之后的总量，不是原始程序的
time = 9
memory = 2

[program]
t1 = a + b        # 注释可以跟在程序行后面
res = t1 * 2
```

**两个指标**：

| 指标 | 定义 |
|------|------|
| `time` | 所有**带算子**的指令的 `costs[op]` 之和。纯拷贝 `v = x` 不花钱；`costs` 里没有的算子抛 `KeyError` |
| `memory` | **同时存活的计算值峰值**（寄存器压力）。只有赋值目标算数，输入变量和字面量免费 |

`memory` 的精确定义（这条必须先钉死再写代码）：一个计算值从**定义它的那条指令之后**开始存活，到**最后一次读它的指令**为止；`res` 是程序输出，存活到结尾。

```
memory = max over i of  |{ v : v 定义在 j <= i，且 (v == "res" 或 v 在某个 k > i 被读) }|
```

推论：一个**从没被读过、也不是 res** 的目标，**花 time 但不占 memory**。这条推论有一条测试专门钉它（`test_a_dead_assignment_costs_time_but_not_memory`），也是 Phase 3 为什么只降 time 不一定降 memory 的原因。

---

## 2. Phase 1 — 用例加载器的两个 bug（目标 8–10 分钟）

```
FAIL  test_costs_are_keyed_by_the_bare_operator
FAIL  test_program_lines_drop_trailing_comments
ERROR test_every_case_file_loads_and_parses
```

**先说这一阶段为什么值钱**（面试官在等这句）：

> "The loader is upstream of everything else. If the cost table is keyed wrong or the program lines carry junk, Phase 2 through 4 will be comparing against numbers that were never right — I'd be debugging the analyzer for a bug that isn't there. So this gets fixed first and I'm not moving on until every case file round-trips through parse_program."

### Bug #1：cost 的 key 没 strip

```python
if section == "costs":
    key, _, value = line.partition("=")
    costs[key] = int(value)          # ← "+ = 1" 的 key 是 "+ "，带一个尾空格
elif section == "expected":
    key, _, value = line.partition("=")
    expected[key.strip()] = int(value)   # ← 这一支反而 strip 了
```

`int(" 1")` 是合法的，所以 **value 那边看起来没问题**，坏的只有 key。两个分支写法不一致 —— 这正是这类 bug 能活过 code review 的原因。后果是 `costs["*"]` 直接 `KeyError`，而你会以为是自己的 `analyze` 写错了。

**口述**：

> "`'+ = 1'.partition('=')` gives me `'+ '` with the trailing space. The expected-section branch strips its key and the costs branch doesn't — same shape, different behaviour, which is why it reads as fine. Fix is `costs[key.strip()]`."

### Bug #2：只过滤了整行注释，没管行尾注释

```python
line = raw.strip()
if not line or line.startswith("#"):    # ← 只挡住了 '#' 开头的行
    continue
```

`case_04_mixed.txt` 里的 `k = 4 * 5              # folds to 20` 会原样进 `program`。然后 `parse_program` 拿到 5 个 token 而不是 3 个，直接 `ValueError`。

**口述**：

> "Comments are stripped only when they start the line. A trailing comment survives into the program text, so the right-hand side has five tokens instead of three and the parser rejects it. One line fixes both cases: `line = raw.split('#', 1)[0].strip()` before the blank check — a full-line comment just becomes empty and gets skipped by the existing guard."

**这两个都不要用 AI 生成修复**，各是一行。自己改完跑测试，把"上游 bug 会污染下游全部结论"这句话说出来 —— 这是 Phase 1 的全部得分点。

---

## 3. Phase 2 — 分析器（目标 15 分钟）

`time` 是直加，**真正的题在 `memory`**：它是一道活跃区间（liveness）题。

**先说建模**：

> "Time is a straight sum. Memory is a liveness question: each computed value occupies a register from its definition to its last read, so peak pressure is the largest overlap of those intervals. `res` never dies because whoever runs the program reads it. Single assignment means each name has exactly one interval, so I don't need a real dataflow pass — one backward scan for last-use, then a sweep."

```python
last_use = {}
for i, instr in enumerate(program):
    for operand in instr.operands():
        if not is_literal(operand):
            last_use[operand] = i           # 正向扫，最后一次覆盖就是 last use

defined_at = {}
for i, instr in enumerate(program):
    defined_at.setdefault(instr.target, i)

peak = 0
for i in range(len(program)):
    live = sum(
        1 for name, birth in defined_at.items()
        if birth <= i and (name == OUTPUT or last_use.get(name, -1) > i)
    )
    peak = max(peak, live)
```

测试钉死的形状：

| 测试 | 程序 | memory |
|------|------|--------|
| `test_a_linear_chain_needs_one_register` | `t1=a+b; t2=t1*c; res=t2-d` | 1（累加器风格） |
| `test_a_balanced_tree_needs_two` | `t1=a+b; t2=c+d; res=t1*t2` | 2 |
| `test_pressure_grows_with_the_number_of_pending_values` | 三个待用中间值 | 3 |
| `test_inputs_and_literals_are_free` | `res = a + 1` | 1（只有 `res`） |
| `test_res_stays_live_to_the_end` | `res=a+b; unused=c+d` | 1 |

**常见错法**：把 `memory` 写成"目标变量总数"或"同时存在的变量数（含输入）"。契约说得很死 —— 只有赋值目标，且要看 last use。写之前先把上面那个公式念一遍。

> **复杂度**：上面这版是 O(n²)（每条指令扫一遍 `defined_at`）。**主动说出还能做 O(n)**：把每个值变成 `+1` 在 def 处、`-1` 在 last_use 之后的差分事件，扫一遍取前缀和最大值。n 小的时候不值得写，但要让面试官知道你看得见。

---

## 4. Phase 3 — 死代码消除（目标 10 分钟）

从 `res` 出发做**反向可达性**，到不动点。

```python
defined_at = {instr.target: i for i, instr in enumerate(program)}
needed, stack = set(), [OUTPUT]
while stack:
    name = stack.pop()
    if name in needed or name not in defined_at:
        continue                     # 已处理，或者是输入变量
    needed.add(name)
    for operand in program[defined_at[name]].operands():
        if not is_literal(operand):
            stack.append(operand)
return [instr for instr in program if instr.target in needed]
```

**关键在"到不动点"**，不是一趟反向扫：

```
t1 = a + b
t2 = c + d
junk = t1 * t2     ← 没人读 junk
res = t1 - e
```

删掉 `junk` 之后，`t2` 的唯一读者也没了 —— `t2` 跟着变死。`test_removal_cascades_backwards` 就是抓这个。用 worklist（或者反向不动点迭代）天然处理，用"一趟从后往前"就会漏。

**口述**：

> "Dead code elimination is reachability on the def-use graph, rooted at `res`. It has to be a fixed point rather than one reverse pass, because removing an instruction can orphan the ones that only fed it. A worklist from `res` gives me that for free, and literals and input variables are just leaves that keep nothing alive."

其它边界：保序（`test_preserves_the_original_order`）、程序里没有 `res` → 返回 `[]`、字面量操作数不留活口。

**要主动指出的一点**：DCE **必然降低 time，但不保证降低 memory** —— 死目标本来就不占寄存器（见 §1 的推论）。`test_eliminating_dead_code_lowers_the_reported_time` 只断言 time 变小，就是这个原因。

---

## 5. Phase 4 — 常量折叠（目标 15 分钟）

正向走一遍，维护 `known: Dict[str, int]`：能替换的操作数替换成字面量，两个操作数都是字面量就直接算出来，结果写成 `target = <value>` 并记进 `known`，让常量继续往下传。

```python
known, out = {}, []
for instr in program:
    left = str(known[instr.left]) if instr.left in known else instr.left
    right = ... # 同上
    if instr.op is None:
        if is_literal(left):
            known[instr.target] = int(left)
        out.append(Instr(instr.target, left)); continue
    if is_literal(left) and is_literal(right):
        value = _apply(instr.op, int(left), int(right))
        if value is not None:
            known[instr.target] = value
            out.append(Instr(instr.target, str(value))); continue
        known.pop(instr.target, None)          # 除零：交给运行时
    out.append(Instr(instr.target, left, instr.op, right))
```

**单赋值让这题简单了一大截，这句要说**：

> "Because every target is assigned exactly once, a value never changes after it's computed. That means plain forward propagation with a dict is sound — I don't need an iterative dataflow analysis with a lattice and a meet operator. If the language allowed reassignment or had branches, this same code would be wrong."

### 三个埋在这一阶段的坑

**① 整数除法必须向零截断，不是 Python 的 `//`。**

```python
-7 // 2   ==  -4      # Python：向负无穷取整
-7 /  2   ==  -3      # C / 大多数被编译语言：向零截断
```

`test_integer_division_truncates_toward_zero` 四个方向全测（`-7/2`、`7/-2`、`-7/-2`、`7/2`）。正确写法：

```python
magnitude = abs(a) // abs(b)
return -magnitude if (a < 0) != (b < 0) else magnitude
```

**不要写 `int(a / b)`** —— 大整数会先掉进 float，精度就没了。

**② 除以零不折叠**：原样输出，并把这个 target 从 `known` 里拿掉（否则后面会拿着一个不存在的常量继续传）。

**③ 折叠本身不删任何指令。** `test_folding_alone_removes_nothing` 钉死这点。删除是 DCE 的活 —— 两个 pass 各干各的，这是编译器 pass 设计的基本纪律。

### 顺序：先折叠，后 DCE

```python
def optimize(program, costs):
    folded = fold_constants(program)
    live = eliminate_dead_code(folded)
    return live, analyze(live, costs)
```

> "The order isn't arbitrary. Folding rewrites uses of `gain` into the literal 18, and *that's* what makes `gain = k - 2` dead. Run DCE first and you'd keep both. Real pipelines run these to a fixed point for the same reason — one pass exposes work for the other."

`case_04_mixed.txt` 就是这条链的完整演示：

```
k = 4 * 5              → k = 20
gain = k - 2           → gain = 18
noise = a * b          → 死
tmp = noise + 1        → 死
scaled = pixel * gain  → scaled = pixel * 18     (pixel 是输入，留下)
res = scaled + 1
```

优化后只剩两条，`time = 7 + 2 = 9`，`memory = 1`。

---

## 6. 收尾：你自己该补的测试

- `optimize` 的**单调性**：优化后的 time / memory 都不能变大（测试里有一条，值得主动提）
- 折叠后的程序再折叠一次应该是**幂等**的
- `analyze` 在 `costs` 缺算子时抛 `KeyError`（已有），缺 `res` 的程序 DCE 成 `[]`（已有）
- 同一个变量被赋值两次（违反 SSA）→ 契约没定义，主动说"我会拒绝并报错"
- 极长的线性链（几万条）→ `analyze` 的 O(n²) 会现原形，这时才值得换差分扫描

---

## 7. Prompt 序列

见 [`prompts.md`](./prompts.md)。
