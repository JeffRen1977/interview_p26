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
- Phase 3：还在 `itertools.combinations(cards, 3)` 上枚举 → O(n³)。点数值域只有 1–9，应该按**点数频次**枚举 `r1<=r2<=r3` 的 84 种组合，用组合数展开计数。

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

**陷阱**

- Phase 2：用 DFS 找"一条路径"当成"最短路径"交上去。无权图最短路 = BFS，说出这句话本身就是分。
- Phase 3：把"收集 k 把钥匙"拆成 `k!` 段两点 BFS。正确状态是 `(row, col, key_mask)`，复杂度 O(R·C·2^k)。
- 渲染：路径标记必须只覆盖 `.`，不能覆盖 `S`/`E`/钥匙字符。

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
