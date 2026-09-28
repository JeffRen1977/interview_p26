# Maze Solver (C++) — 迷宫寻路 Q1–Q5 全阶梯

> Python 版 [`../maze_solver/`](../maze_solver/) 的 C++17 复刻。契约、Phase 划分、埋的 bug 一致；**两个 bug 的"失败方式"在 C++ 里完全不同**（见 §2）。`project/` 是**面试开局状态**。

```bash
cd maze_solver_c++
make test                     # 测 project/ → 3 FAIL + 26 ERROR / 32
make test IMPL=solution       # 测 solution/ → 32 OK
make clean
```

Phase 4 对应面经里 **2026 年 4 月新加的 Q5**（炸弹 / 带权路径）。到 Q4 是 "strong" 线，Q5 是加分区。

---

## 1. 题面与契约

```
project/
├── grid.h/.cpp      # Coord(row, col)、kDirections、图例与代价常量（已给）
├── maze.h/.cpp      # Maze：walls / start / end / keys / gates
│                    #   neighbors / raw_neighbors / terrain_cost / is_bombable（已给）
├── renderer.h/.cpp  # render(maze, path)  ← Phase 1 bug #1
└── solver.h/.cpp    # dfs_reachable       ← Phase 1 bug #2
                     # shortest_path             （Phase 2 桩）
                     # shortest_path_all_keys    （Phase 3 桩）
                     # min_energy_path           （Phase 4 桩）
tests/test_maze_solver.cpp
    # Phase1RenderAndDfs / Phase2ShortestPath / Phase3KeysAndScale / Phase4EnergyAndBombs
```

图例：`#` 墙，`.` 通路，`S` 起点，`E` 终点，`a`–`d` 钥匙，`A`–`D` 对应的门，`~` **崎岖地形**（Phase 4 才有意义：能走，但一步 5 点能量）。

（门只到 `D`：`E` 已经被终点占用了 —— 这类"字符命名撞车"本身就是面试里值得指出的细节。）

```cpp
bool dfs_reachable(const Maze&, const Coord& start, const Coord& goal);
std::optional<std::vector<Coord>> shortest_path(const Maze&);
std::optional<std::vector<Coord>> shortest_path_all_keys(const Maze&);
std::optional<std::pair<int, std::vector<Coord>>> min_energy_path(const Maze&, int bombs = 0);
std::string render(const Maze&, const std::vector<Coord>& path = {});
```

邻居顺序是契约的一部分：**上、下、左、右**（`kDirections`）。改它会让最短路的具体走法变化。

**返回类型的一个设计点**：Python 用 `Optional[List[Coord]]`，C++ 这里用 `std::optional<std::vector<Coord>>` 而不是"空 vector 表示失败" —— 因为**空路径和不可达是两件事**（`start == end` 时路径是 `{start}`，长度 1；不可达是 `nullopt`）。用哨兵值把两者混在一起，是这道题最容易埋下的契约债。

---

## 2. 先讲清楚：同样的 bug，C++ 的失败方式不一样

这是本复刻最值得带走的一课。两个 Phase 1 bug 在 Python 里都会**大声崩溃**，在 C++ 里都会**安静地给你错误结果**。

| bug | Python 的表现 | C++ 的表现 |
|---|---|---|
| 行列写反 | 非正方形迷宫上 `IndexError`，测试直接报错 | `std::string::operator[]` **不做边界检查** → 未定义行为 |
| DFS 缺 `visited` | `RecursionError`，可捕获、可报告 | **栈溢出 → 段错误**，整个测试进程一起死 |

所以这个复刻做了两件事，都要在面试里说出来：

**① 有 bug 的 `render` 加了一道显式边界保护**：

```cpp
const auto r = static_cast<std::size_t>(coord.col);   // BUG：行列写反
const auto c = static_cast<std::size_t>(coord.row);
// std::string::operator[] does not bounds-check, so guard before writing.
if (r >= grid.size() || c >= grid[r].size()) continue;
```

这行保护本身是**好实践**，但它把 bug 藏得更深了：非正方形迷宫上不再崩溃，而是**路径干脆没画出来**。Python 会给你一个指着出错行的 traceback，C++ 给你一张缺了星号的图。

> **这就是防御性编程的双刃**：它把"崩溃"换成了"静默错误"。加保护没错，但**保护不该替代断言** —— 如果那个坐标本来就不该越界，正确的写法是 `assert` 或抛异常，而不是 `continue`。这句话说出来，比修好 bug 本身更值钱。

**② `dfs_reachable` 加了显式递归深度上限**：

```cpp
// C++ has no recursion limit of its own: an unguarded DFS on a cyclic grid
// overflows the stack and takes the whole test binary down with it, with no
// exception to catch. This explicit limit stands in for Python's
// RecursionError so a runaway search is reportable instead of fatal.
inline constexpr int kRecursionLimit = 4000;
```

超限时抛 `std::runtime_error`，于是测试报 ERROR 而不是整个进程被 SIGSEGV 带走。**Python 的递归上限是白送的安全网，C++ 没有。**

---

## 3. Phase 1 — 两个经典 bug（目标 8–12 分钟）

```
FAIL  test_render_marks_the_path
FAIL  test_render_on_a_non_square_maze
FAIL  test_render_never_hides_start_end_keys_or_gates
ERROR test_dfs_reachable_terminates_on_a_loop_free_answer
        recursion limit exceeded in dfs_reachable
```

### Bug #1：行列写反（renderer.cpp）

```cpp
const auto r = static_cast<std::size_t>(coord.col);   // ← 转置了
const auto c = static_cast<std::size_t>(coord.row);
```

**口述**：

> "It indexes by column first. On a square maze that silently draws the transposed path — the diff shows the stars mirrored across the diagonal — and on a 3x7 maze the guard swallows every write, so the path just doesn't appear. `Coord` is `(row, col)`, so the access has to be `grid[row][col]`."

这就是为什么测试里**故意放了一个非正方形迷宫**（3×7 的 `CORRIDOR`）—— 正方形迷宫会把这个 bug 藏起来。这句话说出来是加分项。

### Bug #2：DFS 缺 visited（solver.cpp）

```cpp
for (const Coord& next : maze.neighbors(start)) {
    if (dfs(maze, next, goal, depth + 1)) return true;   // A→B→A→B… 永不终止
}
```

**口述**：

> "There's no visited set, so two adjacent open cells bounce forever — it's not a depth problem, a three-cell corridor already exhausts the limit. In C++ there is no RecursionError, so without the explicit guard in this file it would be a stack overflow and a dead test binary."

修复：加 `std::set<Coord>& visited` 参数（放在内部 helper 上，**public 签名不动**），进入时 `visited.insert(start)`，递归前 `if (!visited.count(next))`。

> **C++ 细节**：`visited` 要按**引用**传递。按值传会在每层拷贝整个集合 —— 既慢又错（兄弟分支之间看不到彼此的访问记录，仍然会指数爆炸）。这是从 Python 直译过来最容易出的一处，因为 Python 传的本来就是引用。

---

## 4. Phase 2 — BFS 最短路（目标 15 分钟）

**先说选型**：

> "The grid is unweighted, so BFS is optimal and Dijkstra would be overkill — DFS would return *a* path, not the shortest one. I'll keep a parent map and rebuild the route when I reach the end."

```cpp
std::map<Coord, Coord> parent;      // child -> parent
std::set<Coord> seen{start};        // seen doubles as visited
std::deque<Coord> queue{start};
while (!queue.empty()) {
    const Coord current = queue.front();
    queue.pop_front();
    for (const Coord& next : maze.neighbors(current)) {
        if (seen.count(next)) continue;
        seen.insert(next);
        parent[next] = current;
        if (next == end) return rebuild(next);
        queue.push_back(next);
    }
}
return std::nullopt;
```

测试钉死的四件事：

| 断言 | 意思 |
|------|------|
| 5×5 迷宫上 `path->size() == 5` | 真的是最短，不是"某条路" |
| `shortest_path(BLOCKED) == nullopt` | 不可达返回 `nullopt`，不是空 vector、不是抛异常 |
| `maze.end = maze.start` → `{start}` | 起点=终点的定义要说清楚 |
| `GATE_ON_THE_ONLY_ROUTE` → `nullopt` | 这个函数不捡钥匙，所以门就是墙 |

**两个 C++ 坑**：

1. **`const Coord current = queue.front(); queue.pop_front();`** —— 必须先按值拷贝再 pop。写成 `const Coord& current = queue.front()` 再 `pop_front()`，引用当场悬空。这是 AI 生成 BFS 代码里出现频率最高的一处 UB。
2. **入队时就标记 `seen`，不是出队时**。出队才标记会让同一格重复入队，在大网格上是数量级差异（`test_shortest_path_beats_any_depth_first_walk` 用 21×21 全开网格量它）。

---

## 5. Phase 3 — Bitmask BFS（目标 15–18 分钟）

需求变了：**所有钥匙都是必经点**，门 `A`–`D` 在拿到对应小写钥匙前等同于墙。

**先说为什么朴素做法不行**：

> "Two things break plain BFS. First, 'nearest key first' is greedy and not optimal. Second, enumerating all k! key orders with pairwise BFS blows up. The fix is to fold the key set into the state: BFS over `(cell, mask)`. Every edge still costs 1, so BFS stays optimal, and the state space is `R × C × 2^k` — 81×81×16 is about 105k states."

```cpp
using KeyState = std::pair<Coord, int>;
const KeyState origin{start, pickup(maze, start, 0)};
std::map<KeyState, KeyState> parent;
std::set<KeyState> seen{origin};
std::deque<KeyState> queue{origin};
```

**最容易错的一行**：`seen` / `parent` 必须以 `(cell, mask)` 为键。只按 cell 去重会把"带着新钥匙再走一遍同一格"剪掉 —— 而那恰恰是唯一合法路线。

`test_gate_forces_a_detour_and_the_route_revisits_cells` 就是抓这个：

```
#######        路线：S(1,1) → 下取 a → 原路返回 → 穿过 A → E
#S.A.E#        长度 9，且 unique(path).size() < path.size()
#.#####
#a....#
#######
```

> **C++ 细节**：`std::pair<Coord, int>` 直接可以当 `std::map` 的 key —— 因为 `Coord` 定义了 `operator<`，`std::pair` 的字典序比较会自动组合。如果你想用 `unordered_map`，就得手写 `std::hash` 特化，而**给一个 pair 写 hash 时忘掉 mask 那一维，就复现了上面那个 bug**。用有序容器在这里既省事又安全，这个取舍值得说。

---

## 6. Phase 4 — Dijkstra + 炸弹预算（目标 15–18 分钟）

> 面经里 Q5 可能只出其中一半（要么炸墙、要么带权最短路），**开场先跟面试官确认触发条件和代价模型**。

- 踏入普通格子花 1 点，踏入 `~` 花 **5** 点；
- 踏入一堵**内部**墙 = 炸开它，花 `kBombCost = 4` 点并**消耗一颗炸弹**；
- **外框是基岩**，永远炸不开（`maze.is_bombable` 已经帮你判好）；
- 起点不收费，费用按"每迈出一步"结算；钥匙仍是必经点。

### 先说为什么 BFS 直接作废

> "Up to Phase 3 every edge cost 1, which is the only reason BFS was optimal. Now a step onto rough terrain costs 5 and a blast costs 4, so the queue no longer visits states in cost order. That's Dijkstra."

```
#######      BFS 的答案：5 格直线，能量 16
#S~~~E#      Dijkstra 的答案：7 格绕行，能量 6
#.....#
#######
```

BFS 会自信地返回那条穿泥地的直线 —— 它不是慢，是**错**。

### 状态第三次变大

| Phase | 状态 | 算法 |
|-------|------|------|
| 2 | `cell` | BFS |
| 3 | `(cell, keys_mask)` | BFS |
| 4 | `(cell, keys_mask, bombs_left)` | **Dijkstra** |

炸弹**不改地图**，只花预算 —— 这是让状态空间不爆炸的关键建模决策，一定要说出来：

> "I'm charging the blast at the moment I step into the wall, so the grid never changes and I don't have to track *which* walls were destroyed. `bombs_left` is all the extra state I need."

### 换成 Dijkstra 后，两条规矩跟着变

```cpp
while (!heap.empty()) {
    const HeapEntry entry = heap.top();
    heap.pop();
    const auto it = dist.find(entry.state);
    if (it == dist.end() || entry.cost > it->second) continue;   // 陈旧堆项
    if (entry.state.coord == end && entry.state.mask == goal_mask) {
        return {entry.cost, rebuild(entry.state)};               // ← pop 时判终点
    }
    ...
}
```

1. **终点判定从 push 时挪到 pop 时**。Phase 2 里"入队看到终点就返回"是对的（BFS 的层序保证），**Dijkstra 里这样写会返回次优解**。这是这一阶段最容易被面试官抓的一行。
2. **`seen` 变成 `dist`**。不再是"见过就跳过"，而是"这次更便宜才重新展开"。

### C++ 专属的一处：`priority_queue` 默认是最大堆

```cpp
struct HeapEntry {
    int cost;
    EnergyState state;
    // std::priority_queue is a MAX-heap, so invert the comparison to pop the
    // cheapest first. Tie-break on the state to keep the order deterministic.
    bool operator<(const HeapEntry& o) const {
        return cost != o.cost ? cost > o.cost : o.state < state;
    }
};
```

**`std::priority_queue` 弹出的是"最大"元素**，和 Python 的 `heapq`（最小堆）相反。直接把 Python 代码直译过来会得到一个"从最贵的状态开始搜"的算法 —— 它会跑，会返回结果，结果是错的。

另外注意 tie-break：代价相同的两个状态，比较器必须给出**确定的**顺序，否则同一份输入在不同运行下可能返回不同的（同样最优的）路径，测试就飘了。

### 测试钉死的取舍

| 测试 | 形状 | 考点 |
|------|------|------|
| `test_prefers_the_long_cheap_detour_over_rough_terrain` | `~` 直线 vs 绕行 | BFS 作废，必须 Dijkstra |
| `test_blasts_when_the_detour_is_more_expensive` | 绕行 12，炸开 11 | 有炸弹**该用** |
| `test_keeps_the_bomb_when_walking_around_is_cheaper` | 绕行 6，炸开 7 | 有炸弹**不该用** —— 炸弹是选项不是义务 |
| `test_bomb_budget_is_enforced_across_the_whole_walk` | 两堵墙 | 预算是**整条路**的，不是每步的 |
| `test_the_outer_border_is_bedrock` | 给 5 颗炸弹 | 不能炸穿外框抄近道 |
| `test_degenerates_to_bfs_when_every_step_costs_one` | 无 `~` 无炸弹 | **退化自检**：必须与 Phase 2/3 结果一致 |

最后一条是最有价值的自检：**当所有边权都是 1，Dijkstra 的答案必须等于 BFS 的答案**。写完立刻跑它。

### 如果面试官出的是"半径爆破"版本

面经提到另一个变体：`get_affected_area(coord, radius)` —— 引爆会摧毁半径内的**所有**墙。**这个版本的状态空间会炸**，你要能立刻指出来：

> "If a blast clears a radius, the set of destroyed walls becomes part of the state — two walks that spent the same bombs in different places are no longer interchangeable, so the state is `(cell, mask, bombs_left, destroyed_set)` and that's exponential. In practice you either (a) restrict detonation to the cell you're standing on and let the blast be consumed immediately, which collapses it back to what I have here, or (b) precompute for each candidate blast site which regions it connects, and search over blast sites instead of cells. I'd ask which model you want before I write anything."

**这段话本身就是 Q5 的分。**

---

## 7. 收尾：你自己该补的测试

- 路径合法性 walker（测试里的 `assert_valid_walk` / `assert_valid_blast_walk`）：**主动指出"我用不变量校验而不是硬编码期望路径"** —— 迷宫从 Q2 走到 Q5 换了三次算法，硬编码期望的测试每次都要重写，不变量校验一行不用改
- 起点就是终点、起点就站在钥匙上
- 只有一格通路的 1×N 迷宫
- 迷宫非矩形 → `parse_grid` 应该抛 `std::invalid_argument`（已给的实现里有）
- 用 sanitizer 跑一遍：`make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"` —— 专抓 §4 那个 `queue.front()` 悬空引用

---

## 8. Prompt 序列

见 [`prompts.md`](./prompts.md)。
