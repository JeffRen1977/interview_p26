# AI-Native Coding — Meta AI-Enabled Coding 面试专区

Meta 的 AI-Enabled Coding 轮是 **60 分钟、单项目、3–4 个 Checkpoint** 的工程模拟：
给你一个陌生的小工程 + 一套预置单测，考察**代码库定位、AI 协同、测试防御**三件事。
通过线是至少完整跑通 Phase 1–3。

## 先读这两篇

| 文档 | 内容 |
|------|------|
| [`playbook.md`](./playbook.md) | **step-by-step 打法**：60 分钟时间盘、每个 Checkpoint 的六步循环、5 个 prompt 模板、反模式清单、临场英文话术、两天冲刺计划 |
| [`question_bank.md`](./question_bank.md) | 收集到的真实样题原文 + 考点拆解 + 8 类可能变体 |

## 再练这些工程（都能直接跑）

| 样题 | 目录 | Phase 1 → 2 → 3 |
|------|------|------------------|
| Card Game（和为 15） | [`card_game/`](./card_game/) | multiset 校验 bug → 贪心结算 → 频次表 + 蒙特卡洛 |
| Maze Solver | [`maze_solver/`](./maze_solver/) | 行列写反 + 缺 visited → BFS 最短路 → Bitmask BFS 收钥匙 |
| Max Unique Characters | [`max_unique_chars/`](./max_unique_chars/) | sanitize 大小写 bug → 回溯 → 掩码状态去重 |
| RateLimiter Engine | [`ratelimiter_engine/`](./ratelimiter_engine/) | 6 阶段工程题：滑窗 / 多租户 / 热更新 / 降级 |
| TTL KV + Tag Index | [`ttl_kv_index/`](./ttl_kv_index/) | TTL 存成绝对时间 + get 不惰性过期 → tag 扫描查询 → 倒排索引 |
| TTL KV + Tag Index (C++) | [`ttl_kv_index_c++/`](./ttl_kv_index_c++/) | 同上，C++17：`erase` 替代 `delete`，value 为 `std::string` |
| Friend Recommendation | [`friend_recommendation/`](./friend_recommendation/) | 4 阶段：校验 bug → 共同好友排序 → precision/recall/coverage → 2-hop 扩展 |

除 RateLimiter 外结构一致：

```
<sample>/
├── README.md    # 题面、契约、每个 Phase 的打法与参考算法
├── prompts.md   # 逐阶段可复制的 prompt
├── project/     # 面试开局状态：Phase 1 的 bug 真的埋着，Phase 2/3 是 stub
├── solution/    # 参考实现
└── tests/       # spec：Phase1* / Phase2* / Phase3* 三组测试类
```

```bash
cd card_game                                    # 或 maze_solver / max_unique_chars / ttl_kv_index / friend_recommendation
python3 -m unittest discover -s tests -v                     # 练：测 project/（开局是红的）
python3 -m unittest discover -s tests -k Phase1 -v           # 只跑当前 Phase
AINC_IMPL=solution python3 -m unittest discover -s tests -v  # 对答案：测 solution/

# C++ 复刻
cd ttl_kv_index_c++
make test                     # 测 project/
make test IMPL=solution       # 测 solution/
```

RateLimiter 的跑法不同（实现直接在 `project/` 里）：

```bash
cd ratelimiter_engine/project
python3 -m unittest tests.test_ratelimiter -v
```

## 开局状态自检

| 样题 | `project/` 应该红成这样 | `solution/` |
|------|------------------------|-------------|
| card_game | 2 FAIL + 17 ERROR / 25 | 25 OK |
| maze_solver | 2 FAIL + 16 ERROR / 21 | 21 OK |
| max_unique_chars | 3 FAIL + 10 ERROR / 16 | 16 OK |
| ttl_kv_index | 4 FAIL + 11 ERROR / 20 | 20 OK |
| ttl_kv_index_c++ | 4 FAIL + 11 ERROR / 20 | 20 OK |
| friend_recommendation | 2 FAIL + 17 ERROR / 24 | 24 OK |

FAIL 是 Phase 1 埋的真 bug，ERROR 是 Phase 2/3 还没实现的 `NotImplementedError`。

## 练习方式

1. 起一个 60 分钟计时器，只开 `project/` 和 `tests/`，**不要看 `solution/`**；
2. 按 `playbook.md` 的六步循环走，每个 Phase 结束跑测试并 `git commit`；
3. 全程出声用英文解释你在干什么 —— 这一轮的分一半在沟通上；
4. 跑完再对照 `solution/` 和样题 README 复盘，重点看你是在哪个 Phase 超时的。
