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

## Prompt 7 — 收尾 review

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
