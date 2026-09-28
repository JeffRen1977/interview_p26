# Meta AI-Enabled Coding — 样题库与考点拆解

收录目前拿到的真实样题（面经整理）+ 同类变体。每题给出：工程结构、三个 Phase 的任务、常见陷阱、以及本仓库对应的可跑复刻工程。

> 共同点：所有题都是**"已有工程 + 预置单测"**，不是白板。Phase 1 必有埋好的 bug，Phase 3 必有规模/边界对抗。

---

## 样题 1：Card Game — 和为 15 的策略判定系统

**工程结构**

- `Card`：点数 1–9 + 花色
- `Table`：桌面卡牌池
- `GameEngine`：规则校验器
- `Strategy`：策略接口

**Phase 1（Bug Fix）**
`is_valid_move` 有逻辑漏洞：允许同一张卡被复用两次，或选了不在桌面上的卡牌。定位并修复，让断言测试通过。

**Phase 2（Baseline Strategy）**
实现基础贪心/暴搜策略：从当前桌面反复找出所有和为 15 的 3 张卡组合并结算得分，直到无合法操作。

**Phase 3（Strategy Optimization）**
引入对手机制或多次抽卡模拟；优化查找算法（Hash / 频次表加速三数之和），在多轮 Monte Carlo 模拟中跑进限定时间。

**陷阱**

- Phase 1：用 `card in table.cards` 做值相等判断 → 重复消费同一张牌照样通过；正确做法是 **multiset（Counter）包含性检查**。
- Phase 3：还在 `itertools.combinations(cards, 3)` 上枚举 → O(n³)。点数值域只有 1–9，应该按**点数频次**枚举 `r1<=r2<=r3` **且和为 15** 的组合 —— 一共只有 **13** 组，用组合数展开计数。（注意别和 `C(9,3)=84`「1–9 里所有严格递增三元组」混了，那是另一个数。）

**复刻工程**：[`card_game/`](./card_game/)

---

## 样题 2：Maze Solver with Path Printing — 迷宫寻路与回溯输出

**工程结构**

- `Grid`：坐标解析器
- `Maze`：地图对象
- `renderer`：打印渲染模块

**Phase 1（Debug Warm-up）**
两个 bug：① 迷宫打印路径时的格式偏移（行列写反）；② DFS 中漏写 `visited` 集合导致无限递归 / Stack Overflow。

**Phase 2（Shortest Path Feature）**
改用 BFS（或 Dijkstra）求起点到终点最短路，并在地图上用 `*` 标出完整路径轨迹。

**Phase 3（Multi-Checkpoint / Obstacles Scalability）**
迷宫变成大规模动态网格，增加必经中继点（Key/Gate 机制）。需要识别出朴素两点 BFS 已不适用，改用**状态压缩 BFS（Bitmask）**或分段最短路以避免 TLE。

**Phase 4 / Q5（2026 年 4 月新加，加分区）**
两种出法之一：① 用 `get_affected_area()` 炸掉半径内的墙；② 引入不等边权（崎岖地形、能量消耗），改用 **Dijkstra** 求最小代价路径。**开场先跟面试官确认触发条件与代价模型**——面经明确提到这点。

> 面经给出的官方阶梯是 **Q1→Q5**：Q1 打印覆盖 `S`/`E`（禁 AI）→ Q2 缺 `visited` 死循环 → Q3 方向门 `>`/`<`（踩到只能朝指定方向走，改 `get_neighbors`）→ Q4 钥匙/门 → Q5 炸弹或带权。**到 Q4 是 "strong" 线。**

**陷阱**

- Phase 2：用 DFS 找"一条路径"当成"最短路径"交上去。无权图最短路 = BFS，说出这句话本身就是分。
- Phase 3：把"收集 k 把钥匙"拆成 `k!` 段两点 BFS。正确状态是 `(row, col, key_mask)`，复杂度 O(R·C·2^k)。
- 渲染：路径标记必须只覆盖 `.`，不能覆盖 `S`/`E`/钥匙字符。
- **Q5 最容易被抓的一行**：Dijkstra 里把终点判定留在 push 时（Phase 2 的 BFS 写法）→ 返回次优解。终点必须在 **pop 时**判，`visited` 也要从"见过就跳"换成 `dist` 表"更便宜才重展开"。
- **Q5 的建模分**：炸弹按"踏入墙格时结算"，地图不变，状态只多一维 `bombs_left`。如果是**半径爆破**版本，被摧毁的墙集合会进入状态 → 指数级，要能立刻说出这句话并给出两条收敛做法。

**复刻工程**：[`maze_solver/`](./maze_solver/)

---

## 样题 3：Maximize Unique Characters / Substring Set — 字典树与回溯剪枝

**工程结构**
输入一组词表，要求选取字符互不重叠的子集，使去重后的字符总数最大（LeetCode 1239 的工程化版本）。

**Phase 1 ~ Phase 2**
从词表中过滤非法字符（大小写、非字母、自身含重复字符的词），实现基础回溯搜索。

**Phase 3**
测试集提供两组对抗样本：**大量短词** 与 **少量超长词**。需要根据数据特征用 Trie 剪枝或位运算掩码（bitwise OR）压榨性能。

**陷阱**

- Phase 1：`sanitize` 忘记先 `lower()` 就做去重判断 → `"Aa"` 被当成合法词，后面 bitmask 全错。
- Phase 2：用 `set` 做状态并每层拷贝 → 常数巨大。
- Phase 3：只换成 bitmask 但不加**上界剪枝**（`当前长度 + 剩余可能长度 <= best` 就剪）→ 24 个词仍然 2²⁴ 爆炸。
- 别忘了"少量超长词"这组：瓶颈不在子集数，而在**单词自身去重与非法字符过滤**，Trie/掩码预处理一次即可。

**复刻工程**：[`max_unique_chars/`](./max_unique_chars/)

---

## 样题 4：RateLimiter Engine — 多租户限流与降级（非算法型）

不是算法题，而是**工程契约题**：滑动窗口、租户隔离、权重 cost、规则热更新、存储故障降级，共 6 个 stage。用来练"AI 协同 + 分阶段交付"最合适。

**复刻工程**：[`ratelimiter_engine/`](./ratelimiter_engine/)

---

## 样题 5：TTL KV Index — 带过期淘汰的内存 KV + Tag 查询引擎

轻量级内存数据库 / 缓存索引：进程内 Key-Value，每条记录可带 TTL 和一组 tag，再按 tag 做 AND/OR 过滤。

**工程结构**

- `FakeClock`：可注入时钟（测试把起点钉在 t=1000）
- `Store`：`set` / `get` / `delete` / `exists` / `size`
- `query` / `query_fast`：按 tag 过滤 live keys

**Phase 1（Bug Fix）**
两处 TTL 契约违反：① `set` 把 duration 当成绝对时间戳写入 `expires_at`；② `get` 完全不看过期，过期 key 照样返回 value。定位并修复，让断言测试通过。

**Phase 2（Tag Query）**
实现扫描版 `query(tags, match="all"|"any")`：AND / OR、空 tags 的真空真假、过期跳过、覆盖写入替换整组 tags。

**Phase 3（Inverted Index）**
4 万 key 上对一个稀有 tag 连查 3000 次。必须识别出 O(n) 扫描不可用，改为 `tag → set(keys)` 倒排，并在 set / delete / 惰性过期时维护索引，否则会返回幽灵 key。

**陷阱**

- Phase 1：只修 `get` 不改 `expires_at = now + ttl` → 测试时钟从 1000 起，带 TTL 的 key 会立刻被判死。
- Phase 2：空 tags + `match="all"` 返回 `[]`。空 AND 为真，应返回全部 live keys。
- Phase 3：把 `query_fast = query` 包一层。压力循环是 3000 × 40k 次扫描，过不了时间。更隐蔽的漏：覆盖写入忘了从旧 tag 的 posting list 里删 key。

**复刻工程**：[`ttl_kv_index/`](./ttl_kv_index/)（Python）· [`ttl_kv_index_c++/`](./ttl_kv_index_c++/)（C++17）

---

## 样题 6：Friend Recommendation — 社交图 People-You-May-Know

题池里**唯一的图论题**，官方难度标 Easy，但有 4 个 Checkpoint。

**工程结构**

- `social_graph.py`：`SocialGraph`，无向 / 对称 / 非自反，`friends()` 返回只读邻接视图
- `recommenders.py`：`is_valid_recommendations` / `recommend_baseline` / `evaluate` / `recommend_fast`

**Phase 1（Bug Fix）**
`is_valid_recommendations` 要保证五条规则：①user 存在 ②不推自己 ③不推已有好友 ④不重复 ⑤候选必须在图里。当前实现漏掉两条。

**Phase 2（Baseline Strategy）**
按**共同好友数** `|friends(user) ∩ friends(c)|` 排序，分数 ≥ 1 才有资格，分数降序 + id 升序 tie-break，取前 k。

**Phase 3（Measure Quality）**
离线评估：给一组 holdout 边，算 `precision@k` / `recall@k` / `coverage` 的均值。`recommend_fn` 必须是参数，好让任何推荐器都能进同一管道。

**Phase 4（Scale）**
4 万用户、平均度 4、查 2000 次。O(N·deg) 扫全图约 14 秒，预算 1.5 秒。

**陷阱**

- Phase 1 bug #1 是**查错了一边**：`candidate in graph.friends(candidate)` —— 图非自反，这个分支**永远不触发**，规则 3 是死代码。正确是 `graph.friends(user)`。
- Phase 1 bug #2 是规则 5 **压根没写**。规则 3 和 5 重叠但不等价（好友必在图里，图里的不一定是好友），两个 check 都要留。
- Phase 2：`mutual == 0` 的人不能进结果，否则陌生人会被推上来。
- Phase 3：precision 的分母是 **k**，不是 `len(recs)` —— 只返回 1 个且命中，k=4 时是 0.25。coverage 是独立指标，专抓"因为几乎不出手所以很准"的推荐器。
- Phase 4：关键恒等式 `f ∈ friends(user) 且 c ∈ friends(f) ⟺ f ∈ friends(user) ∩ friends(c)` —— **"有多少个我的好友指向 c"本身就是共同好友数**，Counter 一遍 2-hop 即可，不用集合求交。
- Phase 4 的加分点是**主动说反例**：2-hop 的开销是 `Σ deg(f)`，一个 8000 好友的名人就让单次查询走 8000 步，比扫表还慢。生产做法是按度数截断，或用 **Adamic-Adar**（`1/log(deg(f))`）加权。

**复刻工程**：[`friend_recommendation/`](./friend_recommendation/)

---

## 样题 7：Compiler Optimization — 三地址码的代价分析与优化

题池里**最难的一道**（官方标 Hard，4 个 Checkpoint）。考 **DAG 依赖分析 + 活跃区间**，不考编译器前端。

**工程结构**

- `optimizer_utils.py`：`load_case`（用例加载器）+ `Instr` / `parse_program` / `is_literal`（已给）
- `compiler_optimizer.py`：`analyze` / `eliminate_dead_code` / `fold_constants` / `optimize`
- `tests/data/case_*.txt`：每个文件三段 —— 算子 cost 表、**优化后**期望的 `(time, memory)`、程序正文

**程序模型**：`target = left op right`，算子只有 `+ - * /`，**每个变量只赋值一次**（SSA），输出固定是 `res`，从不被赋值的变量是输入。

**Phase 1（Bug Fix）**
`load_case` 解析错了，导致后面所有测试拿着错的基准比。

**Phase 2（Basic Analyzer）**
`time` = 带算子指令的 cost 之和（纯拷贝免费）；`memory` = **同时存活的计算值峰值**（寄存器压力），只有赋值目标算数，输入变量和字面量免费，`res` 存活到结尾。

**Phase 3（Dead Code Elimination）**
从 `res` 反向可达性剪枝，**必须迭代到不动点**。

**Phase 4（Constant Folding）**
正向传播 `known` 表，两个字面量操作数就地求值并继续往下传。

**陷阱**

- Phase 1 bug #1：`"+ = 1".partition("=")` 的 key 是 `"+ "`，costs 分支忘了 `strip()` 而 expected 分支没忘 —— **同一个 if/elif 两支写法不一致**，所以能活过 code review。后果是 `costs["*"]` 直接 KeyError，而你会以为是 `analyze` 写错了。
- Phase 1 bug #2：注释过滤只挡了 `startswith("#")`，**行尾注释**活到了 `parse_program` 里变成 5 个 token。修复是 `raw.split("#", 1)[0].strip()` 放在空行判断之前。
- Phase 2：把 `memory` 写成"目标变量总数"或"含输入变量的同时存在数"。契约是 last-use 区间的最大重叠。**推论：从没被读过又不是 `res` 的目标，花 time 但不占 memory** —— 这也是 DCE 只保证降 time 的原因。
- Phase 3：用"一趟从后往前扫"而不是 worklist。删掉 `junk` 会让只喂它的 `t2` 跟着变死，**级联**必须迭代到不动点。
- Phase 4 最大的坑：**整数除法要向零截断（C 语义），不是 Python 的 `//`**。`-7 / 2` 是 -3 不是 -4。写 `abs(a)//abs(b)` 再补符号，**别写 `int(a / b)`**（大整数掉 float）。
- Phase 4 其二：除以零不折叠，原样输出并把 target 从 `known` 里拿掉。
- Phase 4 其三：**折叠不删任何指令**，删除是 DCE 的活。而且顺序必须是 **先折叠后 DCE** —— 折叠把 `gain` 的使用点改写成字面量，`gain = k - 2` 才变死；反过来跑就删不掉。
- 加分点：主动说"整个设计靠单赋值才成立 —— 有分支或重复赋值就得上真正的 dataflow 分析（格 + meet 算子）"；以及"`memory` 是值压力的下界，不等于寄存器分配结果，真实分配还要处理干涉与溢出"。

**C++ 版差异**（[`compiler_optimization_c++/`](./compiler_optimization_c++/)）

- **除法方向不再是坑**：C++ 的整数 `/` 从 C++11 起就保证向零截断，直接写 `a / b` 就对。坑换成了**整数宽度** —— `std::stoi("4000000000")` 抛 `out_of_range`，`int` 运算是有符号溢出 UB，必须 `std::stoll` + `long long`。
- **新的 UB 面**：worklist 里 `const std::string& name = stack.back();` 再 `pop_back()` 是悬空引用（AI 生成代码的高频错法）；`LLONG_MIN / -1` 溢出要和除零一起挡掉。
- **异常继承关系**：`std::out_of_range` / `std::invalid_argument` 都继承自 `std::logic_error`，测试 runner 靠这个分类，写自己的测试时要自己 catch。
- **加分动作**：`make test CXXFLAGS="-std=c++17 -g -O1 -fsanitize=address,undefined"` —— 面试里主动跑 sanitizer，比嘴上说"我会注意 UB"强十倍。

**复刻工程**：[`compiler_optimization/`](./compiler_optimization/)（Python）· [`compiler_optimization_c++/`](./compiler_optimization_c++/)（C++17）

---

## 可能遇到的同类变体（同一套打法都能覆盖）

| 变体 | Phase 1 常见 bug | Phase 3 常见优化点 |
|------|------------------|--------------------|
| Word Ladder / 单词接龙 | 邻接生成漏了自身、缺 visited | 双向 BFS、按通配模板建桶 |
| 事件调度器 / 会议室 | 区间边界闭合写错（`<` vs `<=`） | 排序 + 堆，或差分数组 |
| LRU / TTL 缓存 | 访问命中忘记移动到头部 | 双向链表 + dict，惰性过期 |
| 表达式解析器 | 优先级表少一档、括号栈没弹干净 | 单遍 shunting-yard 替代递归重入 |
| 文件系统 / Trie 路径匹配 | 通配符 `*` 匹配空串处理错 | Trie + 记忆化，避免重复回溯 |
| 库存 / 订单撮合 | 部分成交后没回写剩余量 | 价格档位堆化，O(log n) 撮合 |
| 网格洪水填充 / 岛屿 | 递归深度爆栈 | 显式栈 / BFS，或并查集 |
| 版本化 KV / 时间旅行读 | 二分找 `<= t` 时取错边界 | 有序数组二分替代线性扫描 |

**判断你是否准备好**：随便挑上表一行，你能在 30 秒内说出"Phase 1 我会先看什么、Phase 3 我会换成什么数据结构"，就够了。

---

## 使用建议

1. 先读 [`playbook.md`](./playbook.md) 的 §0 时间盘和 §4 prompt 模板；
2. 每道样题**计时 60 分钟**跑一遍，全程出声用英文解释；
3. 跑完对照 `solution/` 复盘，重点看你是不是在 Phase 3 才想起来补边界测试；
4. 至少练两道，直到"跑测试 → 口述根因 → 结构化 prompt → 逐行 review"变成条件反射。
