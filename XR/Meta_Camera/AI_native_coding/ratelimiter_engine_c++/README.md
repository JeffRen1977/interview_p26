# RateLimiter Engine (C++) — 多租户限流与降级

> Python 版 [`../ratelimiter_engine/`](../ratelimiter_engine/) 的 C++17 复刻。**这道题不是算法题，是工程契约题** —— 滑动窗口、租户隔离、权重 cost、规则热更新、存储故障降级、线程安全，共 7 个 Stage。

```bash
cd ratelimiter_engine_c++
make test      # 22 tests, 全绿
make demo      # 12 个线程 × 3 个租户，验证不超卖
make clean
```

**跟其它样题不同**：这里**没有** `project/` vs `solution/` 的拆分 —— `project/` 就是一份完整的参考实现。练法见 §2。

线程安全的验证方式（**面试里主动跑这一条**）：

```bash
make clean && make test CXXFLAGS="-std=c++17 -g -O1 -Wall -Wextra -pthread -fsanitize=thread"
```

---

## 1. 工程结构与契约

```
project/
├── request.h        # Request{user_id, endpoint, timestamp, cost}
├── memory_store.h/.cpp  # 带故障开关的 KV（将来换 Redis）
├── ratelimiter.h/.cpp   # 引擎本体
└── main.cpp             # 多线程 demo
tests/test_ratelimiter.cpp
    # Stage1DefaultWindow / Stage2MultiTenantAndEndpoint / Stage3CostWeighted
    # Stage4SlidingWindow / Stage5DynamicRules / Stage6Degrade / Stage7ThreadSafety
```

```cpp
MemoryStore store;
RateLimiter limiter(store, /*default_limit=*/10, /*default_window_ms=*/1000);
limiter.set_rule("/search", /*limit=*/1, /*window_ms=*/1000, /*degrade_threshold=*/1);
Decision d = limiter.allow(Request{"u1", "/search", /*timestamp=*/0, /*cost=*/1});
```

| 约束 | 为什么它在测试里 |
|------|------------------|
| 返回 `Decision`，不是 `bool` | 网关需要 ALLOW / DEGRADE / DENY 三态 |
| key = `"{user_id}:{endpoint}"` | 租户 A 打爆 `/search` 不能影响租户 B 或 `/upload` |
| 配额存在 `MemoryStore` 里 | "分布式"：进程内存不是权威来源，Redis 能直接替换 |
| `timestamp` 在请求上 | 测试注入毫秒时钟。**引擎绝不读时钟** |
| `cost` 是权重 | 一个请求可以消耗 N 个单位 |
| 窗口是滑动的 | `t` 时刻的事件在 `now - window_ms` 追平它时过期（**严格大于** cutoff 才保留） |
| `set_rule` 是热的 | 下一次 `allow()` 就用新的 limit / window |
| 存储抛异常 → `Degrade` | 网关保持在线，宁可跳过 origin 也不返回 500 |

三态判定：

- **Allow** — `used + cost <= degrade_threshold`
- **Degrade** — 超过软阈值但仍 `<= limit`，**或者**存储抛了异常
- **Deny** — `used + cost > limit`，**且这次请求不消耗任何配额**

---

## 2. 怎么用它练（重点）

这道题的价值不在"看懂参考实现"，而在**分阶段重建它**。

```bash
# 1. 先备份
git stash    # 或者 cp project/ratelimiter.cpp /tmp/

# 2. 把 RateLimiter::allow / allow_locked 的函数体挖空，只留 throw
# 3. 打开 tests/test_ratelimiter.cpp，Stage 类名就是路线图
# 4. 一次只做一个 Stage，做完 make test，只贴失败输出给 AI
```

**如果你一上来 prompt "帮我实现一个限流器"，你会拿到令牌桶 + Redis Lua + 线程池** —— 全是测试没要求的东西。这道题练的就是抵抗这个。

正确的开局 prompt 是"抽 spec，不写代码"（见 [`prompts.md`](./prompts.md) Prompt 0）。

---

## 3. Stage 1–4：单机语义

### 滑动窗口的边界（Stage 4 的全部考点）

```cpp
const long long cutoff = request.timestamp - rule.window_ms;
for (const auto& [ts, cost] : events) {
    if (ts > cutoff) { /* 保留 */ }      // 严格大于，不是 >=
}
```

`test_oldest_event_expires_exactly_at_window_boundary` 钉死这个：limit=2、window=1000，`t=0` 和 `t=500` 各占一格，`t=999` 被拒；**`t=1000` 必须放行**，因为 `t=0` 的事件恰好在这一刻过期。

`>=` 和 `>` 差一个 tick，这是这类题最经典的差一错误。**改之前先把边界念一遍**。

### 被拒的请求不消耗配额（Stage 1）

```cpp
if (projected > rule.limit) {
    store_.set(key, live);        // 只写回"剪枝后的窗口"，不追加本次事件
    return Decision::Deny;
}
```

`test_denied_request_does_not_consume_quota` 连拒两次，第二次仍必须是 `Deny` 而不是别的 —— 如果你把被拒的请求也 append 进去，窗口会越滚越满，恢复不了。

### 引擎不读时钟（Stage 4）

`test_does_not_use_wall_clock` 用 `t=10000` 起步。任何 `std::chrono::system_clock::now()` 都会让它红。**这是可测试性设计的经典范例：把时间变成参数**。

---

## 4. Stage 5：热更新的一个隐蔽陷阱

```cpp
Decision RateLimiter::allow(const Request& request) {
    // 一次性快照，取在 per-key 锁之外
    const Rule rule = rule_for(request.endpoint);
    const std::string key = request.user_id + ":" + request.endpoint;
    std::lock_guard<std::mutex> guard(key_locks_.for_key(key));
    return allow_locked(key, rule, request);
}
```

**`Rule` 按值快照一次，而不是在 `allow_locked` 里反复去查。** 如果中途重新读规则，一次决策就可能**跨两套规则** —— 用旧 window 剪枝、用新 limit 判定，得到一个两边都不成立的结果。

> **口述**：
>
> "I snapshot the rule once, by value, before taking the per-key lock. If I re-read it inside the decision, a concurrent `set_rule` could split one decision across two rules — pruning with the old window and comparing against the new limit. Copying a 20-byte struct is cheaper than reasoning about that."

`test_hot_reload_shorter_window_drops_old_events` 还钉了另一件事：**缩短窗口必须立刻让老事件过期**。因为剪枝是在 `allow()` 里按当前 rule 做的，而不是在写入时做的 —— 写入时剪枝是个诱人的优化，但它会让热更新失效。

---

## 5. Stage 6：降级

两条路径都要返回 `Degrade`：

1. **软阈值**：`used + cost` 超过 `degrade_threshold` 但仍 `<= limit` —— **仍然消耗配额**；
2. **存储故障**：`get` 或 `set` 抛 `StoreError` —— **不消耗配额**（`test_store_recovers` 验证存储恢复后额度还在）。

```cpp
try {
    if (auto stored = store_.get(key)) events = std::move(*stored);
} catch (const StoreError&) {
    return Decision::Degrade;   // fail open, but degraded
}
```

**要说出口的取舍**：存储挂掉时选择 fail-open（放行但降级）而不是 fail-closed（全拒）。这是**产品决策不是技术决策** —— 网关挂掉的代价通常高于短暂超发配额的代价。但它不是普适的：如果这个限流器守的是付费额度或者反滥用，正确答案就反过来了。**主动说出"这取决于我们在保护什么"，比选对更重要。**

---

## 6. Stage 7：C++ 的线程安全（Python 版没有的深度）

Python 版有 GIL 兜底，很多竞争不会真的炸。C++ 没有，所以这个 Stage 在 C++ 里才是真考点。

### 两级锁

```cpp
mutable std::mutex rules_mutex_;                  // 保护规则表
KeyedLocks key_locks_;                            // 每个配额 key 一把锁
```

配额 key 的 **read-modify-write 必须原子**（读窗口 → 剪枝 → 判定 → 写回）。用一把全局锁也对，但那样租户 A 会阻塞租户 B —— 而**租户隔离正是这道题的核心需求**，用全局锁等于在性能上把 Stage 2 又退回去了。

### `KeyedLocks` 里的三个 C++ 细节

```cpp
class KeyedLocks {
    std::mutex guard_;
    std::unordered_map<std::string, std::mutex> locks_;
public:
    std::mutex& for_key(const std::string& key) {
        std::lock_guard<std::mutex> guard(guard_);
        return locks_.try_emplace(key).first->second;
    }
};
```

1. **`std::mutex` 既不可拷贝也不可移动。** 所以必须用 `try_emplace` 原地构造 —— `locks_[key]` 也行（值初始化），但 `insert({key, std::mutex{}})` 编译不过。很多人第一反应会写成 `unordered_map<string, unique_ptr<mutex>>`，那也对，只是多一次堆分配。
2. **`unordered_map` 是基于节点的容器**，所以 rehash **不会**让已有元素的引用失效（只让迭代器失效）。这正是"返回 `std::mutex&` 让调用方长期持有"能成立的前提 —— 换成 `std::vector` 就彻底错了。**这一句是本题最值钱的 C++ 知识点。**
3. **锁的顺序**：`guard_` 只在查表时持有，返回后立刻释放，然后调用方才去锁 key 自己的 mutex。绝不能在持有 `guard_` 的同时去锁 key —— 那会把所有租户串行化，白做。

### 测试怎么验

| 测试 | 验什么 |
|---|---|
| `test_same_tenant_never_oversells_under_contention` | 16 线程 × 20 请求，limit=50 → **恰好 50**（不超卖也不少卖） |
| `test_tenants_keep_independent_quota_under_contention` | 两个租户各 8 线程 → 各自恰好 10 |
| `test_hot_reload_races_with_traffic` | `set_rule` 与 `allow` 并发 200 轮 → 不看数字，看 **TSan 报不报** |

最后一条是专门为 sanitizer 写的。C++17 没有 `std::barrier`（那是 C++20），测试里手写了一个三行版本 —— **必须让所有 worker 都到齐再开跑**，否则测的是启动抖动而不是竞争。

---

## 7. 收尾：你自己该补的

- `cost` 为负 → `std::invalid_argument`（已有）
- `degrade_threshold > limit` → `std::invalid_argument`（已有）
- 存储在 `get` 成功但 `set` 失败时的行为（当前：Degrade 且不消耗）
- `window_ms` 极大 / `limit = 0` 的退化行为
- 主动提一句 **Redis 版的差别**：现在的 get-modify-set 在单进程里靠 per-key mutex 保证原子，换成 Redis 之后跨进程就不成立了 —— 要么用 Lua 脚本把整段逻辑推到服务端，要么用 `INCR` + `EXPIRE` 换一种窗口近似。**做不完也要说清方案，说清楚也能拿分。**

---

## 8. Prompt 序列

见 [`prompts.md`](./prompts.md)。
