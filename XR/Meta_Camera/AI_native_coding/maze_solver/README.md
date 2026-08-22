# Maze Solver with Path Printing — 迷宫寻路与回溯输出

> 样题 2 的可跑复刻。`project/` 是**面试开局状态**：两个 Phase 1 bug 真的埋在代码里，Phase 2/3 是 `NotImplementedError` 桩。

```bash
cd maze_solver
python3 -m unittest discover -s tests -v                       # 测 project/ → 2 FAIL + 16 ERROR
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 21 OK
```

---

## 1. 工程结构与契约

```
project/
├── grid.py      # Coord(row, col)、parse_grid、DIRECTIONS、图例常量，不许改
├── maze.py      # Maze：walls / start / end / keys / gates、neighbors(coord, keys_mask)，不许改
├── renderer.py  # render(maze, path)  ← Phase 1 bug #1
└── solver.py    # dfs_reachable       ← Phase 1 bug #2
                 # shortest_path             （Phase 2 桩）
                 # shortest_path_all_keys    （Phase 3 桩）
tests/test_maze_solver.py   # Phase1RenderAndDfs / Phase2ShortestPath / Phase3KeysAndScale
```

图例：`#` 墙，`.` 通路，`S` 起点，`E` 终点，`a`–`d` 钥匙，`A`–`D` 对应的门。
（门只到 `D`：`E` 已经被终点占用了 —— 这类"字符命名撞车"本身就是面试里值得指出的细节。）

Public API：

```python
dfs_reachable(maze, start, goal) -> bool
shortest_path(maze) -> Optional[List[Coord]]            # 含首尾；不可达返回 None
shortest_path_all_keys(maze) -> Optional[List[Coord]]   # 收齐所有钥匙后到达 E
render(maze, path=None) -> str                          # 只把 '.' 标成 '*'
```

邻居顺序是契约的一部分：**上、下、左、右**（`DIRECTIONS`）。改它会让最短路的具体走法变化。

---

## 2. Phase 1 — 两个经典 bug（目标 8–12 分钟）

跑测试你会看到四条红线，其实是两个 bug：

```
FAIL  test_render_marks_the_path                   # 路径画歪了
FAIL  test_render_never_hides_start_end_keys_or_gates
ERROR test_render_on_a_non_square_maze             # IndexError
ERROR test_dfs_reachable_terminates_...            # RecursionError
```

### Bug #1：行列写反（renderer.py）

```python
if grid[coord.col][coord.row] == OPEN:      # ← 转置了
    grid[coord.col][coord.row] = PATH
```

**口述**：

> "It indexes `grid[col][row]`. On a square maze that silently draws the transposed path — the test diff shows the stars mirrored across the diagonal — and on a 3×7 maze it walks off the end of the row list and raises IndexError. `Coord` is `(row, col)`, so the grid access has to be `grid[row][col]`."

这就是为什么测试里**故意放了一个非正方形迷宫** —— 正方形迷宫会把这个 bug 藏起来。这句话说出来是加分项。

### Bug #2：DFS 缺 visited（solver.py）

```python
for nxt in maze.neighbors(start):
    if dfs_reachable(maze, nxt, goal):   # A→B→A→B… 永不终止
        return True
```

**口述**：

> "There's no visited set, so two adjacent open cells bounce forever — it's not a depth problem, a three-cell corridor already overflows the stack."

修复：加 `visited: Optional[Set[Coord]] = None`，进入时 `visited.add(start)`，递归前 `if nxt not in visited`。**不要**改签名的前三个参数。

---

## 3. Phase 2 — BFS 最短路（目标 15 分钟）

`shortest_path(maze) -> Optional[List[Coord]]`，忽略钥匙（`keys_mask=0`，所以门等同于墙）。

**先说选型**：

> "The grid is unweighted, so BFS is optimal and Dijkstra would be overkill — DFS would return *a* path, not the shortest one. I'll keep a parent map and rebuild the route when I pop the end."

```python
parent = {start: None}
queue = deque([start])
while queue:
    cur = queue.popleft()
    for nxt in maze.neighbors(cur):
        if nxt in parent:            # parent 同时充当 visited
            continue
        parent[nxt] = cur
        if nxt == end:
            return rebuild(parent, end)
        queue.append(nxt)
return None
```

测试钉死的三件事：

| 断言 | 意思 |
|------|------|
| `len(path) == 5` on the 5×5 maze | 真的是最短，不是"某条路" |
| `shortest_path(BLOCKED) is None` | 不可达返回 `None`，不是抛异常、不是空列表 |
| `maze.end = maze.start` → `[start]` | 起点=终点的定义要说清楚 |
| `GATE_ON_THE_ONLY_ROUTE` → `None` | 这个函数不捡钥匙，所以门就是墙 |

标准坑：把入队时机写成"出队时才标 visited" → 同一格重复入队；或忘了在找到 end 时立刻返回，导致返回的不是最短。

---

## 4. Phase 3 — Bitmask BFS（目标 15–18 分钟）

需求变了：**所有钥匙都是必经点**，门 `A`–`D` 在拿到对应小写钥匙前等同于墙。

**先说为什么朴素做法不行**：

> "Two things break plain BFS. First, 'nearest key first' is greedy and not optimal. Second, enumerating all k! key orders with pairwise BFS blows up. The fix is to fold the key set into the state: BFS over `(row, col, mask)`. Every edge still costs 1, so BFS stays optimal, and the state space is `R × C × 2^k` — 81×81×16 is about 105k states, trivially fast."

```python
start_mask = pickup(maze, start, 0)
parent = {(start, start_mask): None}
queue = deque([(start, start_mask)])
while queue:
    coord, mask = queue.popleft()
    for nxt in maze.neighbors(coord, mask):      # mask 决定门开不开
        nxt_mask = pickup(maze, nxt, mask)
        if (nxt, nxt_mask) in parent:
            continue
        parent[(nxt, nxt_mask)] = (coord, mask)
        if nxt == end and nxt_mask == goal_mask:
            return [cell for cell, _ in rebuild(parent, (nxt, nxt_mask))]
        queue.append((nxt, nxt_mask))
return None
```

**最容易错的一行**：`visited`/`parent` 必须以 `(cell, mask)` 为键。只按 cell 去重会把"带着新钥匙再走一遍同一格"剪掉 —— 而那恰恰是唯一合法路线。

测试 `test_gate_forces_a_detour_and_the_route_revisits_cells` 就是抓这个：

```
#######        路线：S(1,1) → 下取 a → 原路返回 → 穿过 A → E
#S.A.E#        长度 9，且 len(set(path)) < len(path)
#.#####
#a....#
#######
```

其它边界：

- 门永远开不了（钥匙不存在）→ `None`
- 迷宫里没有钥匙 → 结果必须与 `shortest_path` **完全一致**（退化情形）
- 终点被封死 → `None`
- 81×81 + 4 把钥匙 → 3 秒内

---

## 5. 收尾：你自己该补的测试

- 路径合法性 walker（测试里的 `assert_valid_walk`）：每一步相邻、不穿墙、过门时必须已持钥匙、终点是 E —— **主动指出"我用不变量校验而不是硬编码期望路径"**
- 起点就是终点、起点就站在钥匙上
- 只有一格通路的 1×N 迷宫
- 迷宫非矩形 → `parse_grid` 应该报错而不是静默截断

---

## 6. Prompt 序列

见 [`prompts.md`](./prompts.md)。
