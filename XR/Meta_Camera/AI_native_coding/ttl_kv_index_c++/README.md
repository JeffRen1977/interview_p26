# TTL KV Index (C++) — 带过期淘汰的内存 KV + Tag 查询引擎

> Python 版 [`../ttl_kv_index/`](../ttl_kv_index/) 的 C++ 复刻。契约、Phase 划分、埋的 bug 完全一致。`project/` 是**面试开局状态**。

Python 的 `delete` 在 C++ 里叫 `erase`（`delete` 是关键字）。value 统一为 `std::string`。

```bash
cd ttl_kv_index_c++
make test                     # 测 project/ → 4 FAIL + 11 ERROR / 20
make test IMPL=solution       # 测 solution/ → 20 OK
```

---

## 1. 题面与契约

实现一个进程内 KV：`set` / `get` / `erase` / `exists` / `size`，每条记录可带 **TTL** 和一组 **tag**。然后再做按 tag 过滤的查询。

```
project/
├── clock.h     # Clock / WallClock / FakeClock（已给，不要改）
├── store.h     # 公开 API
└── store.cpp   # Store::set / get / exists / size（Phase 1 bug ×2）
                # Store::query          （Phase 2 桩）
                # Store::query_fast     （Phase 3 桩）
tests/test_ttl_kv_index.cpp  # Phase1TTL / Phase2TagQuery / Phase3Scale
```

```cpp
void set(key, value, ttl = nullopt, tags = {})
optional<string> get(key)          // miss 和过期都返回 nullopt
bool erase(key)                    // Python: delete
bool exists(key)                   // 过期视作不存在
int size()                         // 只计未过期
vector<string> query(tags, match="all"|"any")       // 排序后的 live keys
vector<string> query_fast(tags, match="all"|"any")  // 同样答案，但要扛住压力集
```

时钟是注入的：`Store store(clock)`，`FakeClock clock(1000)`。测试把起点钉在 `t=1000`，所以 **TTL 必须是「从现在起的秒数」**，不是绝对时间戳。

过期判定：`clock.now() >= expires_at` 的瞬间 key 就没了（闭区间）。`ttl = nullopt` 永不过期。覆盖写入会同时替换 value、TTL、tags。

`query` 契约：

| `match` | 空 `tags` | 非空 |
|---------|-----------|------|
| `"all"` | 全部 live keys（空 AND 为真） | 必须拥有**每一个** tag |
| `"any"` | `{}`（空 OR 为假） | 拥有**至少一个** tag |

过期 key 不能出现在结果里。返回值**按 key 排序**。

---

## 2. Phase 1 — TTL 的两个 bug（目标 8–10 分钟）

```
FAIL test_exists_and_size_stay_live_before_ttl
FAIL test_get_returns_none_once_ttl_elapses
FAIL test_expired_at_the_exact_boundary
FAIL test_overwrite_resets_ttl
```

### Bug #1：`set` 把 duration 当成了绝对时间

```cpp
data_[key] = Entry{std::move(value), ttl, ...};
//                                   ^^^ 应该是 nullopt if !ttl else clock.now() + *ttl
```

Clock 从 1000 起。`set("a", "1", 10)` 把 `expires_at` 写成了 `10`，`exists` 一看 `1000 >= 10` 就判死。

**口述**：

> "TTL is a duration from now, not a timestamp. Storing `ttl` directly makes every expiring key look already dead, because the test clock starts at 1000. The fix is `expires_at = ttl ? now() + *ttl : nullopt`."

### Bug #2：`get` 完全不看 TTL

```cpp
return it->second.value;   // ← 过期了照样返回
```

`exists` / `size` 走了 `is_expired`，`get` 没走。所以会出现「exists 说没有，get 还能读到」这种契约分裂。

**口述**：

> "get never consults expiry, so a key past its TTL still returns a value. exists and size already check `is_expired`, so the store disagrees with itself. I'll make get lazy-evict: if `now >= expires_at`, erase the entry and return nullopt."

覆盖写入要**重算** expires_at。`test_overwrite_resets_ttl` 就是钉这个：先 5 秒 TTL，还没到期再写成 10 秒，时钟再走 5 秒必须还活着。

---

## 3. Phase 2 — tag 过滤（扫描即可，目标 12 分钟）

扫描全部 live entries。测试钉死四件事：

- `"all"` 是 AND，`"any"` 是 OR；
- 空 tags 的真空真假（上面那张表）；
- **过期 key 不能进结果**；
- **覆盖写入替换整组 tags**。

扫描是 Phase 2 的正确解。**不要在这一阶段建倒排索引**。

---

## 4. Phase 3 — 倒排索引（目标 15–18 分钟）

| 压力测试 | 形状 | 为什么 Phase 2 会死 |
|----------|------|---------------------|
| `test_repeated_rare_tag_lookups` | 4 万 key，一个只命中 2 条的 tag，查 3000 次 | 每次 O(n) 扫描 ≈ 1.2 亿次访问 |
| `test_multi_tag_intersection_is_from_posting_lists` | 8k 只带 `a`、8k 只带 `b`、1 条同时带 `a` 和 `b` | 扫全表做 AND；倒排是两个 posting list 求交 |

维护 `tag -> unordered_set<key>`。`all` = 求交，`any` = 求并，然后再 lazy-expire。覆盖写入必须先从旧 tag 的 posting list 里摘掉 key。

> **什么时候不该上倒排？** 每个 tag 选择度都接近 1 时，倒排退化成扫表，还多付一份写放大。

---

## 5. Prompt 序列

见 [`prompts.md`](./prompts.md)。
