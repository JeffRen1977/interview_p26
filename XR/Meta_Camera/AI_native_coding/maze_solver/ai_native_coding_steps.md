在 Meta 的 AI-Native Coding（60分钟 Mini-Project）中，面对这类具有现成工程骨架的代码库，考察的核心是：**“架构阅读速度、人机协同分工（Prompting 技巧）、关键瓶颈重构与边界防御（Code Review）”**。

这道题是经典的 **带状态的网格图最短路径/迷宫寻路问题（BFS with Bitmask State）**。以下梳理在面试中如何结合 AI 一步一步推进与解释。

---

### 第一步：5分钟代码架构走读与口头总结（向面试官 Think-Aloud）

在向 AI 提问前，先向面试官快速梳理现有代码结构，建立专业信任：

1. **状态定义 (`SearchState`)：** 状态由三元组 `(row, col, collectedKeysMask)` 唯一确定。由于钥匙只有 `a-d`（4把），掩码范围为 $0 \sim 15$。
2. **算法范式 (`solve`)：** 标准的 **广度优先搜索 (BFS)**，保证首次到达终点 `'E'` 时经过的步数最少。
3. **辅助结构：**
* `visitedStates`: 记录 `(r, c, mask)` 是否被访问，防止死循环。
* `parentByState`: 记录状态转移前驱，用于最终回溯路径 (`reconstructPath`)。


4. **验证与渲染 (`validateAndFindStart` & `renderPath`)：** 包含矩形校验、单起点单终点检查、非法字符校验及路径字符替换。

---

### 第二步：利用 AI 发现现有代码的隐患与潜在升级点（Prompt 实战）

如果面试官让你用 AI 审查或重构代码，不要发“帮我看看这段代码”，而要给出**带有架构维度的结构化 Prompt**：

> **Prompt 示例：**
> “Analyze this C++ `MazeSolver` implementation. Focus on:
> 1. Memory overhead in `std::unordered_map` with custom hash.
> 2. What edge cases are missed in `validateAndFindStart` and BFS termination?
> 3. How will this scale if keys increase from 4 (`a-d`) to 16 (`a-p`) or weights/teleports are added?”
> 
> 

#### 需要向面试官解释的 AI 审查重点：

* **内存与性能瓶颈：**
* 当前使用 `std::unordered_map<SearchState, ...>`，节点分配分散且有 Hash 冲突开销。
* 若地图为 $N \times M$ 且有 $K$ 把钥匙，总状态数为 $N \times M \times 2^K$。在 $K=4$ 时仅有 $16 \times N \times M$ 个状态，完全可以用一个扁平的连续数组 `std::vector<uint32_t>` 或 `std::vector<std::vector<std::array<int, 16>>>` 替代哈希表，实现 $O(1)$ 查找并大幅提升 CPU L1 Cache 命中率。


* **路径回溯隐患：**
* `reconstructPath` 中使用 `currentState = parentByState.at(currentState);`。若因边界异常未记录父节点，会抛出 `std::out_of_range`，工业级代码应增加 `find` 检查或前置断言。



---

### 第三步：Follow-up 演进场景与 AI 协作生成

在 AI-Native 面试中，面试官通常会在后续阶段（Phase 2 / Phase 3）抛出拓展需求。你可以引导 AI 针对性修改并做人工 Review。

#### 演进 1：增加传送门 / 权重边（从 BFS 演进为 Dijkstra / A*）

* **场景：** 某些格子移动耗费不同体力，或者包含传送门（Teleporters）。
* **Prompt 指令：**
> “Refactor `solve` from standard `std::queue` (unweighted BFS) to `std::priority_queue` (Dijkstra). Maintain `dist[row][col][mask]` to handle weighted movement costs.”


* **人工 Review 重点：** 检查 AI 是否在加入优先队列时维护了 `if (new_dist >= dist[nr][nc][nmask]) continue;` 剪枝，避免死循环。

#### 演进 2：钥匙数量增加到 16-26 把（状态爆炸与双向搜索）

* **场景：** $2^{26}$ 无法全量展开，需要从起点与终点双向搜索（Bidirectional Search），或者在关键节点（Start, Keys, Doors, End）之间构建**抽象拓扑图（Abstract Graph）**。
* **Prompt 指令：**
> “Optimize when key count is large: compute shortest paths between all POIs (Start, Keys, Doors, End) using BFS once, then solve TSP / Hamiltonian path with DP on bitmask over POIs.”



---

### 第四步：面试中的 AI-Native 协作沟通模板（示范）

1. **发起修改前：**
* *“现有 BFS 在哈希表上有额外的堆分配开销。我准备让 AI 把 `visitedStates` 和 `parentByState` 改为 3D 扁平数组/向量，以提升内存局部性。”*


2. **AI 输出后（人工 Code Review 讲解）：**
* *“AI 生成的代码把状态展平成了 `(row * width + col) * 16 + mask` 的 1D 索引，这很好。但我发现它在构造 `Position` 时没有检查 `maze.empty()` 的动态边界，我在这里手动加一个边界防御。”*


3. **运行测试与验证：**
* 编写极端测试用例：起点即终点、被门完全锁死的迷宫、无解迷宫、带环形走廊且需要重复经过同一位置（捡了钥匙后再返回）的迷宫。
