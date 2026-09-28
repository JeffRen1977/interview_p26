# Compiler Optimization (C++) — 三地址码的代价分析与优化

> Python 版 [`../compiler_optimization/`](../compiler_optimization/) 的 C++17 复刻。契约、Phase 划分、埋的 bug 完全一致，用例文件是同一批。`project/` 是**面试开局状态**。

```bash
cd compiler_optimization_c++
make test                     # 测 project/ → 2 FAIL + 29 ERROR / 36
make test IMPL=solution       # 测 solution/ → 36 OK
make clean
```

> 测试用相对路径读 `tests/data/`，**要在样题根目录下跑**（`make` 会替你 cd 好）。

---

## 1. 和 Python 版的差异（先看这段）

| | Python | C++ |
|---|---|---|
| 缺算子 cost | `KeyError` | `costs.at(op)` → **`std::out_of_range`** |
| 文件格式错 | `ValueError` | **`std::invalid_argument`** |
| 未实现的桩 | `NotImplementedError` | **`std::logic_error`** |
| 目录枚举 | `Path.glob` | `std::filesystem::directory_iterator` |
| 折叠算术 | 任意精度 `int` | **`long long` + `std::stoll`** |
| 整数除法 | `//` 向下取整，**要手动改成向零截断** | `/` **本来就向零截断**，坑换了个位置 |

> **注意继承关系**：`std::out_of_range` 和 `std::invalid_argument` 都继承自 `std::logic_error`。测试 runner 的 catch 顺序是 `AssertFail` → `std::logic_error` → `std::exception`，所以一个"期望抛 `out_of_range`"的测试必须**自己 catch**，不能让它逃到 runner 手里 —— 否则会被记成 ERROR 而不是 ok。写自己的测试时留意这点。

`Instr` 用空字符串表示"没有算子"：

```cpp
struct Instr {
    std::string target, left, op, right;
    bool has_op() const { return !op.empty(); }
    std::vector<std::string> operands() const;   // op 为空时只有 left
};
```

`Metrics::time` 是 `long long`（cost 表可能很大），`memory` 是 `int`。

---

## 2. 题面与契约

```
project/
├── optimizer_utils.h/.cpp     # load_case                ← Phase 1 bug ×2
│                              # Instr / parse_program / is_literal（已给且正确）
└── compiler_optimizer.h/.cpp  # analyze                  （Phase 2 桩）
                               # eliminate_dead_code      （Phase 3 桩）
                               # fold_constants           （Phase 4 桩）
                               # optimize                 （已给：fold → dce → analyze）
tests/
├── data/case_*.txt            # 与 Python 版同一批用例
└── test_compiler_optimizer.cpp
```

**程序模型**：每条指令是 `target = left` 或 `target = left op right`；操作数是变量或整数字面量；算子只有 `+ - * /`；**每个变量只被赋值一次**（SSA）；输出固定是 `res`；从不被赋值、只作为操作数出现的变量是**输入变量**。

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
| `time` | 所有**带算子**的指令的 `costs.at(op)` 之和。纯拷贝不花钱；未声明的算子抛 `std::out_of_range` |
| `memory` | **同时存活的计算值峰值**（寄存器压力）。只有赋值目标算数，输入变量和字面量免费 |

`memory` 的精确定义（先钉死再写代码）：一个计算值从**定义它的那条指令之后**开始存活，到**最后一次读它的指令**为止；`res` 存活到结尾。

```
memory = max over i of  |{ v : v 定义在 j <= i，且 (v == "res" 或 v 在某个 k > i 被读) }|
```

推论：一个**从没被读过、也不是 `res`** 的目标，**花 time 但不占 memory**。这条推论有测试专门钉（`test_a_dead_assignment_costs_time_but_not_memory`），也是 Phase 3 只保证降 time 的原因。

---

## 3. Phase 1 — 加载器的两个 bug（目标 8–10 分钟）

```
FAIL  test_costs_are_keyed_by_the_bare_operator
        cost table keys are wrong: {"* ":4 "+ ":1 "- ":1 "/ ":10 }
FAIL  test_program_lines_drop_trailing_comments
ERROR test_every_case_file_loads_and_parses
```

**先说这一阶段为什么值钱**：

> "The loader is upstream of everything else. If the cost table is keyed wrong or the program lines carry junk, Phase 2 through 4 will be comparing against numbers that were never right — I'd be debugging the analyzer for a bug that isn't there. So this gets fixed first and I'm not moving on until every case file round-trips through parse_program."

### Bug #1：cost 的 key 没 trim

```cpp
if (section == "costs") {
    const auto pos = line.find('=');
    result.costs[line.substr(0, pos)] = std::stoi(line.substr(pos + 1));  // ← key 是 "+ "
} else if (section == "expected") {
    expected[trim(line.substr(0, pos))] = std::stoi(line.substr(pos + 1));  // ← 这一支反而 trim 了
}
```

`std::stoi` 会跳过前导空白，所以 `std::stoi(" 1")` 正常返回 1 —— **value 那边看起来完全没问题，坏的只有 key**。同一个 `if/else if` 的两支写法不一致，这正是这类 bug 能活过 code review 的原因。后果是 `costs.at("*")` 抛 `std::out_of_range`，而你会以为是自己的 `analyze` 写错了。

失败信息直接把 key 打出来了（`{"* ":4 "+ ":1 ...}`）—— **看到引号里那个尾空格就够了**，别去 gdb。

**口述**：

> "`line.substr(0, pos)` on `'+ = 1'` gives me `'+ '` with the trailing space. `std::stoi` tolerates the whitespace on the value side, so only the key is wrong — and the expected-section branch right below it does trim. Same shape, different behaviour. Fix is `trim(line.substr(0, pos))`."

### Bug #2：只过滤了整行注释，没管行尾注释

```cpp
std::string line = trim(raw);
if (line.empty() || line[0] == '#') {   // ← 只挡住了 '#' 开头的行
    continue;
}
```

`case_04_mixed.txt` 里的 `k = 4 * 5              # folds to 20` 会原样进 `program`。然后 `parse_program` 的 `istringstream` 切出 5 个 token 而不是 3 个，抛 `std::invalid_argument`。

修复是**先砍注释再判空**，一行同时处理两种情况：

```cpp
const auto comment = raw.find('#');
std::string line = trim(comment == std::string::npos ? raw : raw.substr(0, comment));
if (line.empty()) continue;      // 整行注释被砍成空串，被这里挡住
```

**这两个都不要用 AI 生成修复**，各是一行。自己改完跑 `make test`，把"上游 bug 会污染下游全部结论"这句话说出来 —— 这是 Phase 1 的全部得分点。

---

## 4. Phase 2 — 分析器（目标 15 分钟）

`time` 是直加，**真正的题在 `memory`**：它是一道活跃区间（liveness）题。

**先说建模**：

> "Time is a straight sum. Memory is a liveness question: each computed value occupies a register from its definition to its last read, so peak pressure is the largest overlap of those intervals. `res` never dies because whoever runs the program reads it. Single assignment means each name has exactly one interval, so I don't need a real dataflow pass — one scan for last-use, then a sweep."

```cpp
std::unordered_map<std::string, long long> last_use;
for (std::size_t i = 0; i < program.size(); ++i)
    for (const auto& operand : program[i].operands())
        if (!is_literal(operand)) last_use[operand] = (long long)i;   // 正向扫，最后一次覆盖就是 last use

std::unordered_map<std::string, long long> defined_at;
for (std::size_t i = 0; i < program.size(); ++i)
    defined_at.emplace(program[i].target, (long long)i);              // emplace：不覆盖已有

for (std::size_t i = 0; i < program.size(); ++i) {
    int live = 0;
    for (const auto& [name, birth] : defined_at) {
        if (birth > (long long)i) continue;
        const auto it = last_use.find(name);
        const bool read_later = it != last_use.end() && it->second > (long long)i;
        if (name == kOutput || read_later) ++live;
    }
    metrics.memory = std::max(metrics.memory, live);
}
```

C++ 特有的两个细节：

- `defined_at.emplace` 而不是 `operator[]` —— 语义是"第一次定义"，`emplace` 不覆盖，意图更准（SSA 下两者等价，但说出来是分）。
- `last_use` 存 `long long` 而不是 `size_t`，因为要和 `-1`（从没被读过）比较。用无符号会绕回一个巨大的数，**这是这段代码最容易埋 bug 的一处**。

测试钉死的形状：

| 测试 | 程序 | memory |
|------|------|--------|
| `test_a_linear_chain_needs_one_register` | `t1=a+b; t2=t1*c; res=t2-d` | 1（累加器风格） |
| `test_a_balanced_tree_needs_two` | `t1=a+b; t2=c+d; res=t1*t2` | 2 |
| `test_pressure_grows_with_the_number_of_pending_values` | 三个待用中间值 | 3 |
| `test_inputs_and_literals_are_free` | `res = a + 1` | 1（只有 `res`） |
| `test_res_stays_live_to_the_end` | `res=a+b; unused=c+d` | 1 |

**常见错法**：把 `memory` 写成"目标变量总数"或"同时存在的变量数（含输入）"。写之前先把上面那个公式念一遍。

> **复杂度**：这版是 O(n·m)（每条指令扫一遍 `defined_at`）。**主动说出还能做 O(n)**：把每个值变成 def 处 `+1`、last_use 之后 `-1` 的差分事件，扫一遍前缀和取最大值。n 小的时候不值得写，但要让面试官知道你看得见。

---

## 5. Phase 3 — 死代码消除（目标 10 分钟）

从 `res` 出发做**反向可达性**，到不动点。

```cpp
std::unordered_map<std::string, std::size_t> defined_at;
for (std::size_t i = 0; i < program.size(); ++i) defined_at[program[i].target] = i;

std::unordered_set<std::string> needed;
std::vector<std::string> stack{kOutput};
while (!stack.empty()) {
    const std::string name = stack.back();   // 注意：先拷贝再 pop_back
    stack.pop_back();
    if (needed.count(name) || defined_at.count(name) == 0) continue;  // 已处理，或输入变量
    needed.insert(name);
    for (const auto& operand : program[defined_at.at(name)].operands())
        if (!is_literal(operand)) stack.push_back(operand);
}
```

**C++ 特有的一个真陷阱**：`const std::string& name = stack.back();` 拿引用，然后 `stack.pop_back()` —— 引用立刻悬空，后面 `push_back` 触发 reallocation 更是雪上加霜。**必须按值拷贝**。这行是 AI 生成代码里很常见的一处 UB，`-fsanitize=address` 能抓到，肉眼 review 也该抓到。

**关键在"到不动点"**，不是一趟反向扫：

```
t1 = a + b
t2 = c + d
junk = t1 * t2     ← 没人读 junk
res = t1 - e
```

删掉 `junk` 之后，`t2` 的唯一读者也没了 —— `t2` 跟着变死。`test_removal_cascades_backwards` 抓这个。worklist 天然处理，"一趟从后往前"会漏。

**口述**：

> "Dead code elimination is reachability on the def-use graph, rooted at `res`. It has to be a fixed point rather than one reverse pass, because removing an instruction can orphan the ones that only fed it. A worklist from `res` gives me that for free, and literals and input variables are just leaves that keep nothing alive."

**要主动指出的一点**：DCE **必然降低 time，但不保证降低 memory** —— 死目标本来就不占寄存器（见 §2 的推论）。`test_eliminating_dead_code_lowers_the_reported_time` 只断言 time 变小，就是这个原因。

---

## 6. Phase 4 — 常量折叠（目标 15 分钟）

正向走一遍，维护 `known`：能替换的操作数替换成字面量，两个操作数都是字面量就地求值，结果写成 `target = <value>` 并记进 `known` 继续往下传。

```cpp
std::unordered_map<std::string, long long> known;
for (const auto& instr : program) {
    const std::string left = resolve(instr.left, known);
    if (!instr.has_op()) {
        if (is_literal(left)) known[instr.target] = std::stoll(left);
        out.push_back(Instr{instr.target, left, "", ""});
        continue;
    }
    const std::string right = resolve(instr.right, known);
    if (is_literal(left) && is_literal(right)) {
        const auto value = apply_op(instr.op, std::stoll(left), std::stoll(right));
        if (value) {
            known[instr.target] = *value;
            out.push_back(Instr{instr.target, std::to_string(*value), "", ""});
            continue;
        }
        known.erase(instr.target);          // 除零：交给运行时
    }
    out.push_back(Instr{instr.target, left, instr.op, right});
}
```

**单赋值让这题简单了一大截，这句要说**：

> "Because every target is assigned exactly once, a value never changes after it's computed. That means plain forward propagation with a hash map is sound — I don't need an iterative dataflow analysis with a lattice and a meet operator. If the language allowed reassignment or had branches, this same code would be wrong."

### C++ 版的坑和 Python 版**不一样**

**① 除法方向不是坑，溢出才是。**

Python 里 `//` 向下取整，要手动改成向零截断。**C++ 的整数 `/` 从 C++11 起就保证向零截断**，直接写 `a / b` 就是对的。所以这里真正的陷阱换成了：

```cpp
std::stoi("4000000000")            // throws std::out_of_range
int a = 2000000000 + 2000000000;   // 有符号溢出 = UB
```

`test_folding_uses_64_bit_arithmetic` 钉这个：`res = 2000000000 + 2000000000` 必须折成 `4000000000`，`res = 3000000 * 3000000` 必须折成 `9000000000000`。**用 `std::stoll` 解析、`long long` 运算、`std::to_string` 回写**。

**别把除法绕进 double**（`static_cast<int>(a / static_cast<double>(b))`）—— 那既丢精度又把 C++ 已经正确的语义搞错。`test_integer_division_truncates_toward_zero` 的 `-7 / 2` 会立刻抓到（double 版本某些写法给 -4）。

**② `LLONG_MIN / -1` 是 UB**，和除零一样要挡掉：

```cpp
if (b == 0) return std::nullopt;
if (a == std::numeric_limits<long long>::min() && b == -1) return std::nullopt;
```

测试没覆盖这条 —— **主动说出来，主动写上**。这是这一轮里最容易拿到"他真懂 C++"印象分的一处。

**③ 除以零不折叠**：原样输出，并把 target 从 `known` 里 `erase`（否则后面会拿着一个不存在的常量继续传）。

**④ 折叠本身不删任何指令。** `test_folding_alone_removes_nothing` 钉死这点。删除是 DCE 的活 —— 两个 pass 各干各的，这是编译器 pass 设计的基本纪律。

### 顺序：先折叠，后 DCE

```cpp
const auto folded = fold_constants(program);
const auto live = eliminate_dead_code(folded);
return {live, analyze(live, costs)};
```

> "The order isn't arbitrary. Folding rewrites uses of `gain` into the literal 18, and *that's* what makes `gain = k - 2` dead. Run DCE first and you'd keep both. Real pipelines run these to a fixed point for the same reason — one pass exposes work for the other."

`case_04_mixed.txt` 是这条链的完整演示：

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

## 7. 收尾：你自己该补的测试

- `LLONG_MIN / -1` 和 `LLONG_MIN * -1` 不折叠（现有测试没覆盖，**主动补**）
- `optimize` 的**单调性**：优化后 time / memory 都不能变大（已有一条）
- 折叠后的程序再折叠一次应该**幂等**
- 同一个变量被赋值两次（违反 SSA）→ 契约没定义，主动说"我会拒绝并报错"
- 用 `-fsanitize=address,undefined` 跑一遍：`make test CXXFLAGS="-std=c++17 -g -fsanitize=address,undefined"`。**主动提这一句** —— 悬空引用和有符号溢出正是这题最容易踩的两类 UB

---

## 8. Prompt 序列

见 [`prompts.md`](./prompts.md)。Python 版的 prompt 大部分可以直接用，差异见 §1 和 §6。
