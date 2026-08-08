# 设计基于语义缓存（Semantic Cache）的 RAG 系统

> **核心考点：** 用向量相似度复用历史答案 → **省钱（少调 LLM）+ 省时间（降 TTFT）**  
> **必须覆盖：** Vector DB + Cosine、命中阈值、TTL、知识库更新后的批量 Invalidate  
> **关联 Demo：** [semantic_cache.py](./semantic_cache.py)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 必问 / 假设 | 设计影响 |
|------|-------------|----------|
| 命中质量 | 可接受的错误复用率？ | 阈值 0.95 vs 0.98；是否二次校验 |
| 新鲜度 | FAQ 静态 vs 实时库存/政策？ | TTL 长短；强失效 vs 懒失效 |
| 多租户 | 缓存是否跨租户共享？ | 隔离键 `(tenant_id, …)` |
| 个性化 | 答案是否依赖用户权限/地区？ | cache namespace 含 role/locale |
| 成本目标 | 目标 cache hit rate？ | 容量、embedding 模型选型 |

**开场金句：**

> RAG 里最贵的是 LLM 生成，不是检索。语义缓存把「意思相近的问题」映射到已生成答案，用一次 ANN + cosine 换掉一次完整 Prefill/Decode。关键是 **高阈值命中 + 可靠失效**，否则省钱变成幻觉扩散。

---

## 1. 目标与收益模型

| 路径 | 延迟 | 成本 |
|------|------|------|
| 全量 RAG + LLM | 检索 + Prefill + Decode | 每次付 Token |
| **Semantic Cache Hit** | Embed + ANN（毫秒～几十毫秒） | 几乎只付 embedding |
| Exact String Cache | 最快 | 撞库率极低（同义改写 miss） |

**白板粗算：**

```
日请求 Q = 1e7
LLM 成本 C $/req，Hit rate h = 30%
日省钱 ≈ Q × h × C
同时：Hit 路径 TTFT 从数百 ms → 数十 ms
```

---

## 2. 端到端架构

```
User Query
    │
    ▼
┌─────────────────────────────────────────┐
│ 1. Normalize (trim / lower / PII scrub) │
└───────────────────┬─────────────────────┘
                    ▼
┌─────────────────────────────────────────┐
│ 2. Embedding Model → q ∈ R^d            │
└───────────────────┬─────────────────────┘
                    ▼
┌─────────────────────────────────────────┐
│ 3. Vector DB ANN (top-k on Cache Index) │
│    score = cosine(q, cached_query_vec)  │
└───────────────────┬─────────────────────┘
                    │
         ┌──────────┴──────────┐
         │ max_score ≥ θ (0.95)│
         ▼                     ▼
   Cache HIT              Cache MISS
   return answer          ┌──────────────────────┐
   (+ cache headers)      │ 4. RAG retrieve docs │
                          │ 5. LLM generate      │
                          │ 6. Write cache entry │
                          └──────────────────────┘
```

**两个向量索引（勿混）：**

| 索引 | 存什么 | 用途 |
|------|--------|------|
| **KB Index** | 文档 chunk embedding | RAG 检索证据 |
| **Cache Index** | **历史 query** embedding → answer | 语义缓存命中 |

面试一定要说清：缓存比的是 **query≈query**，不是 query≈doc。

---

## 3. Cosine Similarity 与命中策略

### 3.1 公式

$$
\cos(q, c) = \frac{q \cdot c}{\|q\|\,\|c\|}
$$

生产中 embedding 常 **L2 归一化**，此时 cosine = **内积**，ANN（HNSW/IVF）可直接用 IP。

### 3.2 阈值 θ = 0.95

| θ | 召回 | 风险 |
|---|------|------|
| 0.90 | 高 hit | 语义碰撞（不同意图复用答案） |
| **0.95** | 平衡 | 常见默认；需 A/B |
| 0.98 | 更安全 | hit rate 下降 |

**命中规则（推荐）：**

```
candidates = ANN(q, k=5, namespace=tenant)
best = max cosine among candidates with valid TTL & kb_version
IF best.score >= θ:
  IF optional_intent_check(q, best.query):   # 防碰撞
    return best.answer
MISS → RAG + LLM → store(q, answer, meta)
```

### 3.3 Exact + Semantic 双层

1. **L1：** `hash(normalized_query)` 精确命中（Redis）  
2. **L2：** Vector ANN 语义命中  

L1 挡完全重复；L2 挡改写。

---

## 4. Cache Entry 数据模型

```
CacheEntry:
  entry_id
  tenant_id
  query_text          # 可选，审计/调试
  query_vec           # 存 Vector DB
  answer_text
  source_doc_ids[]    # 生成时引用的 chunk，用于定点失效
  kb_version          # 或 content_hash
  created_at
  expires_at          # TTL
  hit_count           # 淘汰策略
  locale / role       # 个性化维度
```

Vector DB payload 带 `entry_id`；对象存储 / KV 存完整 answer（大文本不塞 ANN payload）。

---

## 5. TTL（过期机制）

### 5.1 为什么要 TTL？

即使知识库未显式更新，答案也可能过时（价格、政策、模型语气变更）。TTL 是 **兜底新鲜度**。

### 5.2 策略

| 策略 | 做法 | 适用 |
|------|------|------|
| **固定 TTL** | 写入时 `expires_at = now + 24h~7d` | FAQ / 文档助手 |
| **分层 TTL** | 热点 entry 延长；低频短 TTL | 容量治理 |
| **滑动 TTL** | 每次 hit 续期（需 cap） | 常问问题 |
| **逻辑过期** | 懒检查 `now > expires_at` 则 miss | 实现简单 |
| **主动淘汰** | 后台扫描 / Redis EXPIRE | 控索引膨胀 |

**查找路径：**

```
ANN 召回 → filter expires_at > now → filter kb_version == current → 再比 cosine
```

过期 entry：**懒删除**（miss 时删）+ **定期 compaction**（扫 Vector DB）。

### 5.3 TTL 与 θ 的关系

- 高风险领域（医疗/金融）：**更短 TTL + 更高 θ**  
- 静态手册：**更长 TTL + 0.95**

---

## 6. 知识库更新后的失效（Invalidate）

这是面试官最爱深挖的点。

### 6.1 失效触发

```
Doc update / delete / ACL change
    │
    ▼
Re-chunk + re-embed → update KB Index
    │
    ├─ Strategy A: bump kb_version（租户或全集群）
    ├─ Strategy B: 按 source_doc_ids 定点删缓存
    └─ Strategy C: 语义邻域批量失效（ANN around doc/query）
```

### 6.2 Strategy A — `kb_version` 纪元失效

```
KB publish → kb_version++
Lookup: entry.kb_version != current → 视为 miss
可选：异步 drop 旧 version 的全部向量
```

| 优点 | 缺点 |
|------|------|
| 实现极简、正确性高 | 一次发布清空所有缓存，hit rate 归零 |

适合：**小规模租户、发布不频繁**。

### 6.3 Strategy B — 按引用文档定点失效

生成答案时记录 `source_doc_ids`。

```
on_doc_change(doc_id):
  delete all CacheEntry where doc_id ∈ source_doc_ids
```

| 优点 | 缺点 |
|------|------|
| 精确、保留无关缓存 | 若答案「综合多文档」或未记录引用 → 漏删 |

### 6.4 Strategy C — 语义邻域批量失效（必聊）

文档变更后，与该文档 **语义相近的历史问题** 也可能答错：

```
on_doc_change(doc_id):
  d_vec = embed(updated_doc_summary)   # 或受影响 chunk 中心
  neighbors = CacheIndex.ANN(d_vec, k=K, min_score=θ_inv)  # 如 0.85
  # 或：用「与旧答案相关的 query 集」做种子，再扩展
  batch_delete(neighbors)

  # 更稳妥的 query-centric 变体：
  affected_queries = CacheEntry.filter(source_doc_ids contains doc_id)
  for q_vec in affected_queries:
      near = CacheIndex.ANN(q_vec, k=K, min_score=0.90)
      batch_delete(near ∪ affected_queries)
```

**直觉：** 删掉「问法相近」的缓存簇，而不是只删精确 key。

### 6.5 推荐组合

```
默认：TTL（兜底）
     + source_doc_ids 定点失效（精确）
     + 语义邻域扩展（防漏）
大版本发布：kb_version bump（核武器）
```

---

## 7. 正确性陷阱（Deep Dive）

| 陷阱 | 现象 | 缓解 |
|------|------|------|
| **语义碰撞** | 「取消订阅」命中「取消订单」 | 提高 θ；intent 分类；LLM 二阶确认（贵） |
| **权限泄漏** | A 租户答案被 B 命中 | namespace **强制**含 `tenant_id` |
| **个性化污染** | 同问题不同用户不同价 | key 含 `role/locale/plan` |
| **Embedding 漂移** | 换 embedding 模型后分数不可比 | 模型版本写入 entry；切换时全量 rebuild |
| **Stale RAG 证据** | 缓存答案对，但引用过期 | TTL + doc 失效；UI 标明 cached |

---

## 8. 系统与存储选型

```
Embed Service (GPU/CPU)
    │
    ├─► Cache Vector Index (Milvus / Pinecone / pgvector / FAISS)
    ├─► Answer KV (Redis / DynamoDB)  key=entry_id
    └─► KB Vector Index (独立 collection)
```

| 组件 | 要点 |
|------|------|
| ANN | HNSW 低延迟；过滤条件（tenant、version）尽量 **预过滤** |
| 写入 | LLM 完成后异步写缓存，不阻塞 SSE 尾包 |
| 容量 | LRU / LFU by `hit_count`；TTL 双管齐下 |

---

## 9. 与流式 / 限流的衔接

- **Cache HIT：** 可整段返回，或伪装成极快 SSE（单帧）  
- **HIT 仍计 RPM，TPM 可极低权重**（见 [llm-rate-limiter.md](./llm-rate-limiter.md)）  
- **MISS：** 走 [streaming-chatgpt-backend.md](./streaming-chatgpt-backend.md) 全路径，结束后写入 Cache Index

---

## 10. 10x / 100x Scaling Drill

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1× | 单 ANN 集合混租户 | tenant 分 shard / partition |
| 10× | Embed 服务 QPS | 批处理 embed；小模型；本地 embed cache |
| 100× | 失效风暴（大版本 bump） | 蓝绿双 index + 渐进切换；避免瞬时全清 |

> 「100x 时危险的不是命中路径，而是 **一次知识库全量发布把缓存打空**，流量瞬间全打 LLM。要用蓝绿 cache index 或分批失效。」

---

## 11. 面试口述 3 分钟版

1. **目标：** 相似 query 复用答案，省 LLM 钱和 TTFT。  
2. **路径：** Embed → Cache Vector DB cosine ≥ 0.95 → 返回；否则 RAG+LLM 再写入。  
3. **TTL：** `expires_at` 懒过期 + 定期 compaction；高风险域短 TTL。  
4. **失效：** `source_doc_ids` 定点删 + **ANN 邻域批量 invalidate**；大版本用 `kb_version`。  
5. **风险：** 租户隔离、语义碰撞、embedding 版本；Scale 时防失效风暴。

---

## 12. 参考实现

```bash
cd openai
python3 semantic_cache.py
```

Demo 覆盖：cosine 命中、TTL 过期、`kb_version` 全清、**按向量邻域批量 invalidate**。

---

## 13. 追问清单（自测）

- [ ] 缓存索引存的是 query 向量还是 doc 向量？为什么？  
- [ ] 0.95 从哪来？如何 A/B？  
- [ ] TTL 懒删除 vs Redis EXPIRE 差别？  
- [ ] 文档更新后只删 exact key 会怎样？  
- [ ] 语义邻域失效的种子向量用 doc 还是受影响 query？  
- [ ] 多租户 ANN 如何避免交叉命中？  
- [ ] 换 embedding 模型为什么必须 rebuild？
