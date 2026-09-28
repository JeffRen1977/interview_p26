# Maze Solver — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read grid.py, maze.py, renderer.py, solver.py and tests/test_maze_solver.py.
Do NOT write implementation code.

List:
1. The public API I must implement (exact signatures and return types)
2. What Coord's field order is and where the code depends on it
3. What each test class asserts, as invariants
4. Edge cases the tests imply (unreachable end, start == end, closed gate, non-square grid)
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Two Phase-1 failures:
  FAIL  test_render_marks_the_path        (stars appear mirrored across the diagonal)
  ERROR test_render_on_a_non_square_maze  (IndexError)
  ERROR test_dfs_reachable_terminates...  (RecursionError)

Explain each root cause in one sentence, naming the exact line.
Do NOT propose code yet.
```

---

## Prompt 2 — Phase 1 修复

```
Task: fix renderer.render and solver.dfs_reachable. Two minimal edits.
Constraints:
- Coord is (row, col); the grid is indexed grid[row][col]
- render must leave S, E, keys and gates untouched; only '.' becomes '*'
- dfs_reachable keeps its first three parameters exactly as they are; add an
  optional visited set as a fourth parameter with a None default
- Do NOT change grid.py or maze.py, do NOT convert the DFS to a loop
Return only the two function bodies.
```

跑：`python3 -m unittest discover -s tests -k Phase1 -v`

---

## Prompt 3 — Phase 2 BFS

```
Task: implement shortest_path(maze) -> Optional[List[Coord]] in solver.py.
Constraints:
- Unweighted grid, so BFS; return the cell list including both endpoints
- Use maze.neighbors(coord) so the DIRECTIONS order stays authoritative
- Unreachable -> None. maze.end == maze.start -> [start]
- Ignore keys: keys_mask stays 0, so a gate behaves like a wall
- Mark cells visited when they are ENQUEUED, not when dequeued
- Do NOT modify maze.py, grid.py or renderer.py
Return only the function body plus any private helper it needs.
```

跑：`python3 -m unittest discover -s tests -k Phase2 -v`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code, do not touch Phase 3.
<paste unittest output>
```

---

## Prompt 5 — Phase 3（先说清状态设计再让它写）

```
Requirements changed: every key ('a'..'d') is a mandatory checkpoint, and gate
'A'..'D' is a wall until its key is collected. "BFS to the nearest key, then the
next" is greedy and not optimal, and k! pairwise BFS runs blow up.

Task: implement shortest_path_all_keys(maze) -> Optional[List[Coord]] as a BFS
over the state (coord, keys_mask).
Constraints:
- visited/parent MUST be keyed by (coord, mask), never by coord alone — the
  optimal route revisits cells with a larger key set
- Pick up a key when entering its cell, including the start cell
- Goal is coord == maze.end AND mask == maze.all_keys_mask
- Pass the mask to maze.neighbors so gates open at the right moment
- Return the cell list (drop the masks); unreachable -> None
- Same signature; do NOT change maze.py or shortest_path
Return only the function body plus private helpers.
```

跑：`python3 -m unittest discover -s tests -k Phase3 -v`

---

## Prompt 6 — 性能确认

```
State space is rows * cols * 2^k. For the 81x81 maze with 4 keys that is ~105k
states. Confirm the implementation is O(states) and point at anything inside the
loop that is not O(1) — list scans, repeated maze.at calls, path copies.
Do NOT rewrite unless you find a real one.
```

---

## Prompt 7 — Phase 4：先确认模型，再让它写

面试官抛出炸弹/能量需求时，**第一件事是问清代价模型**，不是开 prompt。确认这四条：
一步的基础代价、崎岖地形的代价、炸一堵墙的代价与是否消耗预算、外框能不能炸。
确认完再喂下面这段。

```
Requirements changed: steps no longer cost the same.
- entering an open cell costs maze.terrain_cost(cell): '~' is 5, everything
  else is 1
- entering an INTERIOR wall costs BOMB_COST and burns one bomb from a total
  budget passed in as `bombs`; maze.is_bombable is False for the outer frame
- the start cell is free; cost is charged per step taken
- keys are still mandatory and gates still need their key

That makes BFS wrong, not just slow: it visits by step count, not by cost, so
it would return the 5-cell line through the mud over a 7-cell detour that
costs less.

Task: implement min_energy_path(maze, bombs=0) -> Optional[Tuple[int, List[Coord]]]
as Dijkstra over (cell, keys_mask, bombs_left).
Constraints:
- heapq keyed by accumulated energy; use maze.raw_neighbors so walls are visible
- The goal test happens when a state is POPPED, not when it is pushed —
  popping is what settles a state in Dijkstra
- Replace the visited set with a dist table: re-expand a state only when the
  new cost is strictly lower
- Skip stale heap entries (cost > dist[state])
- Charge the blast on entry so the grid is never mutated and the destroyed set
  never enters the state
- bombs < 0 raises ValueError; unreachable -> None
- Do NOT change maze.py, shortest_path or shortest_path_all_keys
Return only the new function plus private helpers.
```

跑：`python3 -m unittest discover -s tests -k Phase4 -v`

---

## Prompt 8 — Phase 4 自检（这条最值钱）

```
Verify one property without changing code: on a maze with no '~' and bombs=0,
min_energy_path must return exactly len(shortest_path_all_keys(maze)) - 1 as
its energy. Explain why that has to hold, and tell me which single line would
break it if the goal test were done at push time instead of pop time.
```

期望它说出：所有边权为 1 时 Dijkstra 退化为 BFS，代价 = 步数 = 路径长度 - 1；
push 时判终点会在"第一次触碰"就返回，而那条路不一定是最便宜的。

---

## Prompt 9 — 半径爆破变体（只问，不写）

```
Suppose a bomb instead clears every wall within radius R of the cell where it
is detonated (a get_affected_area(coord, radius) helper is provided). Do NOT
write code. Explain what that does to the state space of my Dijkstra, and give
me two modelling choices that keep the search tractable.
```

期望答案：被摧毁的墙集合进入状态 → 指数级；两条出路是 (a) 只能在当前格引爆且爆开后立即通过，退化回 `bombs_left`；(b) 预计算每个候选爆点连通了哪些区域，搜索在爆点上跑而不是格子上跑。

---

## Prompt 10 — 收尾 review

```
Review solver.py and renderer.py against the tests only.
Call out dead code and any behaviour no test covers.
Do NOT change code unless I ask.
```

然后你自己补一句：

> "One thing I'd flag: `E` can't be a gate letter because the exit already owns
> that character. Right now gates stop at `D`. If the maze format ever needs six
> gates, the legend needs a separate namespace — worth writing down before
> someone adds an `E` gate and gets a very confusing bug."
