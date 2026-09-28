# Maze Solver (C++) — 逐阶段 Prompt

---

## Prompt 0 — 抽 spec（不写代码）

```
Read grid.h, maze.h, renderer.h, solver.h and tests/test_maze_solver.cpp.
Do NOT write implementation code.

List:
1. The public API I must implement, with exact signatures and return types
2. Why the return type is std::optional<std::vector<Coord>> rather than an
   empty vector for "unreachable" -- what two situations would collapse
3. The invariant each of the four test groups is asserting
4. What kRecursionLimit in solver.h is standing in for, and why C++ needs it
   where Python does not
5. What the Phase 4 energy model is: cost per cell, cost per blast, what the
   bomb budget covers, and which walls are bombable
```

---

## Prompt 1 — Phase 1 根因（不写代码）

```
Phase-1 failures:
  test_render_marks_the_path                     FAIL
  test_render_on_a_non_square_maze               FAIL
  test_render_never_hides_start_end_keys_or_gates FAIL
  test_dfs_reachable_terminates_on_a_loop_free_answer  ERROR
      recursion limit exceeded in dfs_reachable

Explain each root cause in one sentence, naming the exact line.
For the renderer, explain why the 3x7 maze fails DIFFERENTLY from the 5x5 one,
and what the explicit bounds guard in that function is hiding.
For the DFS, say why the failure is a depth limit here and would be a segfault
without that guard.
Do NOT propose code yet.
```

---

## Prompt 2 — Phase 1 修复

```
Task: fix render() in renderer.cpp and dfs_reachable() in solver.cpp.

renderer constraints:
- Coord is (row, col); index grid[row][col]
- Keep the bounds guard, but tell me in a comment whether it is still reachable
  after the fix
- Only cells currently equal to kOpen become kPath; S, E, keys and gates keep
  their character

dfs constraints:
- Add a visited set. Pass it BY REFERENCE through the recursion -- passing by
  value copies the set at every level, which is both slow and wrong (sibling
  branches would not see each other's marks)
- Do NOT change the public signature of dfs_reachable; put the visited
  parameter on an internal helper in an anonymous namespace
- Keep the kRecursionLimit guard
Return only the changed functions.
```

跑：`make test`

---

## Prompt 3 — Phase 2 BFS

```
Task: implement shortest_path(maze) in solver.cpp.
Constraints:
- BFS over cells; the grid is unweighted so BFS is optimal and Dijkstra is
  overkill
- Mark a cell as seen when you ENQUEUE it, not when you dequeue it
- Take the front element BY VALUE before pop_front(): a reference into the
  deque dangles the moment you pop
- Keep a parent map and rebuild the route when you reach maze.end
- Include both endpoints; start == end returns {start}
- Unreachable returns std::nullopt, NOT an empty vector
- Use maze.neighbors(cell) with the default mask, so a closed gate is a wall
- Do NOT implement the other two functions
Return only the function plus anonymous-namespace helpers.
```

跑：`make test`

---

## Prompt 4 — 只修这个失败

```
Fix ONLY the failure below. Do not refactor unrelated code.
<paste the failing test name and its message>
```

---

## Prompt 5 — Phase 3（先说清状态设计再让它写）

```
Requirements changed: every key is now a mandatory checkpoint, and gates 'A'..'D'
are walls until the matching lowercase key is collected.

"BFS to the nearest key, then the next" is greedy and not optimal, and
enumerating k! key orders with pairwise BFS blows up. The fix is to fold the key
set into the state and run ONE BFS over (cell, keys_mask). Every edge still
costs 1, so BFS stays optimal; the state space is rows * cols * 2^k.

Task: implement shortest_path_all_keys(maze).
Constraints:
- visited and parent MUST be keyed by (cell, mask), never by cell alone -- the
  optimal route revisits cells with a larger key set
- std::pair<Coord, int> works directly as a std::map key because Coord defines
  operator<; do NOT switch to unordered_map without writing a hash that
  includes the mask
- Pick up a key when ENTERING its cell, including the start cell
- Goal is cell == maze.end AND mask == maze.all_keys_mask()
- Pass the mask to maze.neighbors so gates open at the right moment
- Return the cell sequence (drop the masks); unreachable -> std::nullopt
- Same signature; do NOT change maze.cpp or shortest_path
Return only the function plus anonymous-namespace helpers.
```

跑：`make test`

---

## Prompt 6 — Phase 4：先确认模型，再让它写

面试官抛出炸弹/能量需求时，**第一件事是问清代价模型**，不是开 prompt。确认这四条：
一步的基础代价、崎岖地形的代价、炸一堵墙的代价与是否消耗预算、外框能不能炸。

```
Requirements changed: steps no longer cost the same.
- entering an open cell costs maze.terrain_cost(cell): '~' is 5, else 1
- entering an INTERIOR wall costs kBombCost and burns one bomb from a total
  budget passed in as `bombs`; maze.is_bombable is false for the outer frame
- the start cell is free; cost is charged per step taken
- keys are still mandatory and gates still need their key

That makes BFS wrong, not just slow: it visits by step count, not by cost, so
it would return the 5-cell line through the mud over a 7-cell detour that
costs less.

Task: implement min_energy_path(maze, bombs) as Dijkstra over
(cell, keys_mask, bombs_left).
Constraints:
- std::priority_queue is a MAX-heap. Invert the comparison so the cheapest pops
  first, and add a deterministic tie-break on the state itself
- Use maze.raw_neighbors so walls are visible and can be considered for bombing
- The goal test happens when a state is POPPED, not when it is pushed --
  popping is what settles a state in Dijkstra
- Replace the visited set with a dist table: re-expand a state only when the
  new cost is strictly lower. Skip stale heap entries (cost > dist[state])
- Charge the blast on entry so the grid is never mutated and the destroyed set
  never enters the state
- bombs < 0 throws std::invalid_argument; unreachable -> std::nullopt
- Do NOT change maze.cpp, shortest_path or shortest_path_all_keys
Return only the new function plus anonymous-namespace helpers.
```

跑：`make test`，然后

```bash
make clean
make test IMPL=solution CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"
```

---

## Prompt 7 — Phase 4 自检（这条最值钱）

```
Verify one property without changing code: on a maze with no '~' and bombs = 0,
min_energy_path must return exactly shortest_path_all_keys(maze)->size() - 1 as
its energy. Explain why that has to hold, and tell me which single line would
break it if the goal test were done at push time instead of pop time.
```

期望它说出：所有边权为 1 时 Dijkstra 退化为 BFS，代价 = 步数 = 路径长度 − 1；push 时判终点会在"第一次触碰"就返回，而那条路不一定是最便宜的。

---

## Prompt 8 — 半径爆破变体（只问，不写）

```
Suppose a bomb instead clears every wall within radius R of the cell where it is
detonated (a get_affected_area(coord, radius) helper is provided). Do NOT write
code. Explain what that does to the state space of my Dijkstra, and give me two
modelling choices that keep the search tractable.
```

期望答案：被摧毁的墙集合进入状态 → 指数级；两条出路是 (a) 只能在当前格引爆且爆开后立即通过，退化回 `bombs_left`；(b) 预计算每个候选爆点连通了哪些区域，搜索在爆点上跑而不是格子上跑。

---

## Prompt 9 — 引用与容器审查（C++ 版专属）

```
Review renderer.cpp and solver.cpp for reference/iterator lifetime and container
misuse only:
- any reference into a deque, vector or priority_queue held across pop or push
- any set or map passed by value into a recursion where it should be by
  reference
- any std::priority_queue used as if it were a min-heap
- any operator[] on std::string or std::vector that could be out of range
Report findings with file:line. Do NOT change code.
```

---

## Prompt 10 — 收尾 review

```
Review renderer.cpp and solver.cpp against the tests only.
Call out dead code, any bounds guard that is now unreachable, and any behaviour
no test covers.
Do NOT change code unless I ask.
```

然后你自己补两句：

> "One naming thing I'd flag: `E` can't be a gate letter because the exit already
> owns that character, so gates stop at `D`. If the format ever needs six gates,
> the legend needs a separate namespace — worth writing down before someone adds
> an `E` gate and gets a very confusing bug."

> "And one language thing: the bounds guard I kept in `render` is good practice,
> but it's also what turned the row/col bug from a loud crash into a silently
> missing path. Defensive guards trade a crash for a wrong answer. Where the
> index genuinely can't be out of range, the honest tool is an assert, not a
> `continue`."
