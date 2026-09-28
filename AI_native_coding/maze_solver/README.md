# Maze Solver with Path Printing — 迷宫寻路与回溯输出

> 样题 2 的可跑复刻，**Q1–Q5 全阶梯**。`project/` 是**面试开局状态**：两个 Phase 1 bug 真的埋在代码里，Phase 2/3/4 是 `NotImplementedError` 桩。
>
> Phase 4 对应面经里 **2026 年 4 月新加的 Q5**（炸弹 / 带权路径）。到 Q4 是 "strong" 线，Q5 是加分区。

```bash
cd maze_solver
python3 -m unittest discover -s tests -v                       # 测 project/ → 2 FAIL + 27 ERROR / 32
AINC_IMPL=solution python3 -m unittest discover -s tests -v    # 测 solution/ → 32 OK
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
                 # min_energy_path           （Phase 4 桩）
tests/test_maze_solver.py   # Phase1RenderAndDfs / Phase2ShortestPath
                            # Phase3KeysAndScale / Phase4EnergyAndBombs
```

图例：`#` 墙，`.` 通路，`S` 起点，`E` 终点，`a`–`d` 钥匙，`A`–`D` 对应的门，`~` **崎岖地形**（Phase 4 才有意义：能走，但一步 5 点能量）。
（门只到 `D`：`E` 已经被终点占用了 —— 这类"字符命名撞车"本身就是面试里值得指出的细节。）

Public API：

```python
dfs_reachable(maze, start, goal) -> bool
shortest_path(maze) -> Optional[List[Coord]]            # 含首尾；不可达返回 None
shortest_path_all_keys(maze) -> Optional[List[Coord]]   # 收齐所有钥匙后到达 E
render(maze, path=None) -> str                          # 只把 '.' 标成 '*'

# Phase 4 新增（maze.py / grid.py 里已给）
maze.raw_neighbors(coord) -> Iterator[Coord]   # 含墙的四邻居，Phase 4 要"看得见"墙才能决定炸不炸
maze.terrain_cost(coord)  -> int               # 踏入该格的能量：'~' 是 5，其余是 1
maze.is_bombable(coord)   -> bool              # 只有**内部**墙能炸，外框是基岩
min_energy_path(maze, bombs=0) -> Optional[Tuple[int, List[Coord]]]
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

## 5. Phase 4 — Dijkstra + 炸弹预算（目标 15–18 分钟）

> 这是面经里 **2026 年 4 月新加的 Q5**。面试时它可能只出其中一半（要么炸墙、要么带权最短路），**开场先跟面试官确认触发条件和代价模型**——面经明确提到这一点。

需求又变了，而且这次**换掉了算法本身**：

- 踏入普通格子花 1 点能量，踏入 `~` 花 **5** 点；
- 踏入一堵**内部**墙 = 炸开它，花 `BOMB_COST = 4` 点能量并**消耗一颗炸弹**；`bombs` 是整条路的总预算；
- **外框是基岩**，永远炸不开（`maze.is_bombable` 已经帮你判好）；
- 起点不收费，费用按"每迈出一步"结算；
- 钥匙仍然是必经点，门仍然要钥匙。

```python
min_energy_path(maze, bombs=0) -> Optional[Tuple[int, List[Coord]]]
```

### 先说为什么 BFS 直接作废

> "Up to Phase 3 every edge cost 1, which is the only reason BFS was optimal. Now a step onto rough terrain costs 5 and a blast costs 4, so the queue no longer visits states in cost order — BFS would happily return the 5-cell line through the mud when a 7-cell detour is cheaper. That's Dijkstra: a priority queue keyed by accumulated energy."

`test_prefers_the_long_cheap_detour_over_rough_terrain` 就是把这句话钉死的：

```
#######      BFS 的答案：5 格直线，能量 16
#S~~~E#      Dijkstra 的答案：7 格绕行，能量 6
#.....#
#######
```

测试里同时断言了 `len(shortest_path(maze)) == 5` —— **它在提醒你 Phase 2 的答案在这里是错的**。

### 状态第三次变大

| Phase | 状态 | 算法 |
|-------|------|------|
| 2 | `cell` | BFS |
| 3 | `(cell, keys_mask)` | BFS |
| 4 | `(cell, keys_mask, bombs_left)` | **Dijkstra** |

炸弹**不改地图**，只花预算 —— 这是让状态空间不爆炸的关键建模决策，一定要说出来：

> "I'm charging the blast at the moment I step into the wall, so the grid never changes and I don't have to track *which* walls were destroyed. `bombs_left` is all the extra state I need, and the space stays `rows × cols × 2^k × (bombs+1)` — 61×61 with 4 keys and 1 bomb is about 120k states."

### 换成 Dijkstra 后，两条规矩跟着变

```python
while heap:
    cost, state = heapq.heappop(heap)
    if cost > dist.get(state, cost):
        continue                          # 陈旧堆项，已经用更便宜的代价定过了
    coord, mask, left = state
    if coord == end and mask == goal_mask:
        return cost, [cell for cell, _, _ in rebuild(parent, state)]   # ← pop 时判终点

    for nxt in maze.raw_neighbors(coord):
        if maze.is_wall(nxt):
            if left == 0 or not maze.is_bombable(nxt):
                continue
            step, nxt_left = BOMB_COST, left - 1
        elif not maze.is_open(nxt, mask):
            continue                      # 没钥匙的门
        else:
            step, nxt_left = maze.terrain_cost(nxt), left
        nxt_state = (nxt, pickup(maze, nxt, mask), nxt_left)
        if cost + step < dist.get(nxt_state, cost + step + 1):
            dist[nxt_state] = cost + step
            parent[nxt_state] = state
            heapq.heappush(heap, (cost + step, nxt_state))
```

1. **终点判定从 push 时挪到 pop 时**。Phase 2 里"入队时看到 end 就返回"是对的（BFS 的层序保证），**Dijkstra 里这样写会返回次优解** —— 第一次触碰终点的代价不一定是最小的。这是这一阶段最容易被面试官抓的一行。
2. **`visited` 变成 `dist`**。不再是"见过就跳过"，而是"这次更便宜才重新展开"。

### 测试钉死的取舍

| 测试 | 形状 | 考点 |
|------|------|------|
| `test_prefers_the_long_cheap_detour_over_rough_terrain` | `~` 直线 vs 绕行 | BFS 作废，必须 Dijkstra |
| `test_blasts_when_the_detour_is_more_expensive` | 绕行 12，炸开 11 | 有炸弹**该用** |
| `test_keeps_the_bomb_when_walking_around_is_cheaper` | 绕行 6，炸开 7 | 有炸弹**不该用** —— 炸弹是选项不是义务 |
| `test_bomb_budget_is_enforced_across_the_whole_walk` | 两堵墙 | 预算是**整条路**的，不是每步的 |
| `test_the_outer_border_is_bedrock` | 给 5 颗炸弹 | 不能炸穿外框抄近道 |
| `test_degenerates_to_bfs_when_every_step_costs_one` | 无 `~` 无炸弹 | 退化情形必须与 Phase 2/3 结果一致 |

最后一条是最有价值的自检：**当所有边权都是 1，Dijkstra 的答案必须等于 BFS 的答案**。写完立刻跑它。

### 如果面试官出的是"半径爆破"版本

面经提到另一个变体：`get_affected_area(coord, radius)` —— 引爆会摧毁半径内的**所有**墙。**这个版本的状态空间会炸**，你要能立刻指出来：

> "If a blast clears a radius, the set of destroyed walls becomes part of the state — two walks that spent the same bombs in different places are no longer interchangeable, so the state is `(cell, mask, bombs_left, destroyed_set)` and that's exponential. In practice you either (a) restrict detonation to the cell you're standing on and let the blast be consumed immediately, which collapses it back to what I have here, or (b) precompute for each candidate blast site which cells it connects, and run the search over blast sites instead of cells. I'd ask which model you want before I write anything."

**这段话本身就是 Q5 的分**。能说出"半径爆破让 destroyed set 进入状态"这一句，比闷头写一个错的实现强得多。

---

## 6. 收尾：你自己该补的测试

- 路径合法性 walker（测试里的 `assert_valid_walk`）：每一步相邻、不穿墙、过门时必须已持钥匙、终点是 E —— **主动指出"我用不变量校验而不是硬编码期望路径"**
- 起点就是终点、起点就站在钥匙上
- 只有一格通路的 1×N 迷宫
- 迷宫非矩形 → `parse_grid` 应该报错而不是静默截断

---

## 7. Prompt 序列

见 [`prompts.md`](./prompts.md)。
