这三类题目概括了 Meta AI-Native Coding 面试的真实考察模式。

在这场 60 分钟的面试中，平台提供完整的 IDE 与内置 AI 助手。面试官的评估标准不是“你能不能徒手手写算法”，而是 **“你能否作为 Tech Lead / Senior Engineer 高效、严谨地指挥 AI 完成工业级代码开发与排错”**。

以下是针对这三种题型的**通关节奏、标准协作流程与实战 Prompt 模板**：

---

### 题型 1：从头建立新功能 (Feature Addition on Existing Codebase)

**典型场景：** 仓库已有完整领域实体（如 `Deck`, `Card`, `GameEngine`），要求新增 `find_three_card_sum_15()` 或结算规则。

```
[ 1. 快速扫视接口/实体 ] ──> [ 2. 结构化 Prompt 让 AI 出初版 ] ──> [ 3. 人工 Code Review & 补齐边界 ] ──> [ 4. 补充 Unit Tests 跑通 ]

```

* **步骤 1：定位依赖与契约 (3-5 min)**
* 快速查看现有 Class 暴露的属性和方法（如 `Card.rank`、`Card.suit`、手牌是 `list` 还是不可变容器）。
* 向面试官口头确认需求边界：“三张牌是否允许花色不同？牌堆不足 N 张时如何抛错或降级？”


* **步骤 2：精准指挥 AI (1-2 min)**
* **Prompt 模板：**
> "In our codebase, `Card` has attributes `rank: int` and `suit: str`. Implement a method `find_triplets_sum(cards: List[Card], target: int = 15) -> List[Tuple[Card, Card, Card]]` in `GameEngine`. Use a two-pointer or hash-set approach ($O(N^2)$) to avoid duplicate combinations. Maintain clean typing and docstrings."




* **步骤 3：人工 Review 必查项 (面试官的核心评分点)**
* **重复处理：** 是否正确去重（如三张同样的 5）？
* **对象生命周期与副作用：** 抽牌函数是否意外修改了原 `Deck` 或手牌列表？
* **可扩展性：** 如果后续 $N$ 从 3 变成 $K$，函数结构是否容易泛化为回溯/DP？



---

### 题型 2：扩充半完成的系统框架 (Incomplete Pipeline / Scaffold)

**典型场景：** 标准多文件工程（`main.py`, `solve.py`, `utils.py`, `test.py`, `data/`），核心算法留有 `raise NotImplementedError` 或骨架逻辑。

* **步骤 1：从入口和测试反推数据流 (5 min)**
* 优先打开 `test.py` 和 `data/sample.json`，看输入输出的数据结构（Schema）。
* 向面试官做 **Think-Aloud** 架构总结：“从 `main.py` 来看，数据先经过 `utils.py` 解析，瓶颈在 `solve.py` 中的状态转换逻辑。”


* **步骤 2：分层指挥 AI 补全**
* **Prompt 模板：**
> "I am implementing `solve.py` for this pipeline. Given the `InputData` schema from `utils.py`, implement the missing `solve_core(data: InputData) -> OutputData` method.
> Requirements:
> 1. Handle invalid inputs gracefully according to `utils.ValidationError`.
> 2. Optimize for time complexity under $O(N \log N)$.
> 3. Do not modify the existing function signatures."
> 
> 




* **步骤 3：集成联调与回归测试**
* 不要只跑现成的 `test.py`，主动在控制台加几个边界用例（空数据、极大值、极端对抗分布）。



---

### 题型 3：Debug 坏掉的实现 (Fix Broken Implementations & Failing Tests)

**典型场景：** 运行 `pytest` 报错（超时、AssertionError、IndexError、死锁、并发竞态等）。

* **核心禁忌：** **绝对不要直接把整段代码 + 整个 Traceback 无脑丢给 AI 问“怎么修”**（这会让面试官觉得你缺乏独立排错能力）。
* **标准排错四步法：**
1. **定位根因 (Isolate the bug):** 自己先看失败的测试用例与调用栈，指出问题类型（例如：“测试在第 15 个用例失败，原因是当图存在环路时，`visited` 状态没有正确回溯，导致死循环”）。
2. **定点 Prompt 指挥修复：**
> "In `GraphSearcher.traverse`, the current BFS implementation fails when handling cyclic components because `visited` is checked after popping instead of before pushing to the queue. Refactor lines 45-60 to fix the duplicate push and prevent infinite loops."


3. **防回归检查 (Regression Check):** 修复后，不仅要让失败的测试通过，还要确保之前的正常测试用例没有被破坏（Zero regression）。
4. **主动重构坏味道 (Refactor Bad Smells):** 指出 AI 或原始代码中低效的地方（如无意义的内存拷贝、低效的嵌套查找），手动或指挥 AI 做一步优雅重构。



---

### 面试制胜心法（60 分钟时间分配与协作法则）

| 时间段 | 动作 | 核心要点 |
| --- | --- | --- |
| **00 - 08 min** | **快速破题与架构对齐** | 浏览代码目录，读懂 `test` 与核心类，向面试官口头复述任务目标与边界假定。 |
| **08 - 40 min** | **人机协同分步开发** | 模块化推进（Phase 1 $\to$ Phase 2 $\to$ Phase 3）。每次让 AI 生成 20-40 行核心代码，**人工逐行 Review 边界条件**再合并。 |
| **40 - 55 min** | **压力测试与对抗用例** | 补充边界用例（空集、超大数据规模、并发/多线程约束、Corner cases），观察耗时与内存。 |
| **55 - 60 min** | **总结与复杂度阐述** | 向面试官陈述最终方案的 Time/Space 复杂度、工程权衡及未来演进方向。 |

**与 AI 协作的核心口诀：**

* **AI 是打字员和初稿生成器，你是首席架构师和代码审查员 (Code Reviewer)。**
* 每次采用 AI 建议的代码时，向面试官解释一句：“AI 这段写得很直接，不过这里我需要微调一下它的内存分配/边界检查，避免在大规模数据下产生额外开销。”
