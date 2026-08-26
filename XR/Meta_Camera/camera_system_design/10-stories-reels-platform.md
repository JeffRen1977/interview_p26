# 场景 10：Stories / Reels 全链路影像平台

**典型题：** 设计一个类似 Instagram Stories / Reels 的短视频上传、转码、存储与回放系统。

**核心考点：** 高吞吐写入、异构多码率转码（CPU/GPU/ASIC）、海量小文件的低延迟分发（Hot/Cold 分层）。这是 **SpecSWE 媒体平台题**，不是端侧 ISP——和本目录 01–09 的数据面不同，但约束收口的方法一样：先钉 SLA，再拆控制面/数据面，再当场算 PB 和 Tbps。

**核心矛盾：** 三条 SLA 同时成立——每天 3 PB 写入、Upload-finish → Ready-to-watch P95 < 3s、TTFF P90 < 200ms。任何只优化其中一条的方案都会在白板上被拆掉。

**开场 60 秒：**

> 我把这题切成三块：① Ingest（边录边传 + 控制面/数据面分离）② 转码 DAG（GOP 级并行 + Fast/Slow path）③ 小文件存储与边缘回放（Haystack + 预加载）。我会重点挖 ② 和 ③，因为 3 秒发布和 200ms 起播都死在这两层。先报一组假设，不对随时打断。这题我要证明三件事：ingest 峰值能被 Edge POP 吃掉、60s 视频转码能收进 3s、热点视频不会把 Origin 打穿。

---

## 1. 需求与约束

### Functional

1. **上传与摄取：** 15–90s 短视频，1080p/4K；分块断点续传；录制中即可上传（pre-upload）。
2. **异步处理管线：** 元数据提取、违规审查、缩略图、多分辨率/多编码转码（H.264 / HEVC / AV1，720p / 1080p），产出 HLS/DASH manifest。
3. **回放与分发：** TTFF < 200ms，ABR 平滑切换。

### Non-Functional & Scale（可被挑战的默认假设）

| 量 | 假设 | 备注 |
|----|------|------|
| DAU | 5 亿 | 规模锚点，后面带宽用它 |
| 日新增视频 | 1 亿条 | Stories 24h TTL 会消掉一大块长期存储 |
| 原始均大小 | 30 MB | 日原始上传 $\approx 3$ PB |
| 转码后均大小 | 50 MB（多码率合计） | 日新增存储 $\approx 5$ PB |
| 日播放 | 50 亿次 | 峰值回放数十 Tbps |
| 发布延迟 | P95 < 3s（commit → ready） | 不是"用户点 Post 到文件落盘" |
| TTFF | P90 < 200ms | 含预加载时目标是 0 感知 |
| 可用性 / 持久性 | 99.99%；零丢数据 | 元数据与 blob 分开的故障域 |

**明确不做（主动收窄）：** 推荐 Feed 排序、社交图谱、直播、创作者分析后台。那些是另一道题；这里只保证"视频能被可靠地变成可播的 ABR 流"。

---

## 2. 高层架构

控制面（会话、元数据、编排）和数据面（字节）必须分开画。数据面不经过业务应用服务器。

```
[ Mobile Client ]
   │  1. Init / Direct Chunk Upload (QUIC / HTTP3)
   ▼
[ Upload Gateway / Edge POP ] ──(Stream/Save)──► [ Ingest Blob Buffer ]
   │                                                     │
   │ 2. Notify Ingest Complete                           │
   ▼                                                     │
[ Media Orchestration (DAG Engine) ] ◄───────────────────┘
   │
   ├─► [ Worker: GOP Splitter ]
   ├─► [ Worker: Transcoder (H.264 / HEVC / AV1) GPU/ASIC ]
   ├─► [ Worker: Audio / Thumbnail / Moderation ]
   └─► [ Worker: Manifest Generator & Packager ]
   │
   ▼ 3. Publish metadata
[ Video Metadata DB ] + [ Blob Store (Haystack / Tectonic) ]
   │                                 │
   ▼                                 ▼
[ Metadata Cache ]            [ L1 Edge CDN + L2 Origin Shield ]
   ▲                                 ▲
   │ 4. Fetch Manifest               │ 5. Segment Streaming
   └─────────────────── [ Playback Client ] ────────────────────┘
```

口述一句：**Commit 是同步阻塞的控制信令；转码、鉴黄、高清压制是完全解耦的 DAG。** 用户点 Post 时，前面的 GOP 往往已经在边上了。

---

## 3. 核心子系统

### 3.1 分块断点续传与元数据分离

**Control plane：** Client 向 API Gateway 要 `upload_session_id` 和分块预签名 URL；用户 ID、分辨率、时长、拍摄参数写入 Metadata DB（CockroachDB / 跨区 Postgres）。

**Data plane：** 视频 binary 直推最近的 Edge Ingest Gateway，落入 Ingest Blob Buffer，绕过业务机。

**并行分块：** 按 GOP 边界或 2–4 MB 切块。用 **QUIC (HTTP/3)** 避开 TCP 队头阻塞；弱网 WiFi ↔ 5G 切换靠 Connection Migration，不断连。

**边录边传（Zero-Latency Ingest）：** 录制/加特效时后台就把已编码分块传上去。点 Post 只补尾块 + `Commit`，发布感知延迟趋近 0。

```
Client Record/Edit:  [ Chunk 1 ] ──► Uploaded
                     [ Chunk 2 ] ──► Uploaded
                     [ Chunk 3 ] ──► Uploading...
User clicks Post:    [ Commit ]    ──► Trigger DAG immediately
```

客户端持久化 `uploaded_offsets`；网络恢复后只重传失败块（S3 兼容 Multipart）。

### 3.2 基于 DAG 的异构异步处理

不要单机线性任务。事件驱动的 DAG：

```
                    ┌──► [ GOP Splitter ] ──► [ GPU Transcode ] ──┐
                    │                                             ▼
[ Upload Event ] ───┼──► [ Audio Extract ] ──► [ ASR / Loudness ] ─┼──► [ Packager ] ──► Ready
                    │                                             ▲
                    └──► [ Thumbnail ] ──► [ Moderation / Embedding ] ─┘
```

**Chunk-level 并行：** 60s 视频在 I-Frame 处切成约 15 个 4s 分段，调度到 NVENC / 定制 ASIC。Assembler 聚合 MPD / M3U8。60s 内容的墙钟时间压到 2–3s——这是 P95 < 3s 唯一站得住的算法，不是"买更快的单卡"。

编排用 Temporal 或自研 Async Engine：每个节点有重试、幂等 key（`video_id + variant + gop_idx`）、死信队列。

**多梯级编码（Adaptive Encoding Ladders）：**

| Path | 产出 | 目的 |
|------|------|------|
| **Fast** | H.264 720p 一档 | 立刻对粉丝可见，吃掉 3s SLA |
| **Slow** | HEVC / AV1 + 1080p/4K | 省 30–50% CDN 带宽；完成后无缝更新 Manifest |

H.264 vs HEVC vs AV1 的权衡要主动说：AV1 压缩最好、编码最贵；Fast path 选兼容性和编码速度，Slow path 选带宽。

### 3.3 存储：Haystack / Tectonic 思想

短视频切片是海量小文件。POSIX 文件系统会先被 inode 和随机寻道打死，不是被容量打死。

```
[ Edge POP CDN Cache (L1) ]
       │ Miss
[ Regional Origin Shield (L2, NVMe) ]
       │ Miss
[ Distributed Blob Store ]
 ├── Index (in-memory / RocksDB): key -> (volume, offset, size)
 └── Volume files (100 GB append-only)
```

**Volume 聚合：** 多个 4s needle 顺序追加进 100 GB Volume。读任意切片 **1 次寻道**。索引：`video_chunk_key → (volume_id, offset, size)`。

**冷热生命周期：**

| 层 | 谁 | 冗余 |
|----|----|------|
| Hot（前 48h / Stories 24h） | Regional NVMe + Edge CDN | 3 副本 |
| Cold（历史 / 低播放 Reels） | 纠删码 8+4 | 开销从 3× 降到 1.5× |

Stories 到期是**删除路径**，不是变成 Cold——TTL 扫描必须和索引、CDN 失效一起做，否则会播到幽灵切片。

---

## 4. 回放优化与边缘分发

1. **客户端预加载：** Feed 滑到第 $N$ 条时，预取 $N+1$、$N+2$ 的 Manifest + 前 2 个 GOP（约 1–2s）。上滑时数据已在内存，0 秒起播。TTFF 200ms 是**没打中预加载**时的底线，打中了应该接近 0。
2. **Manifest vs 分片缓存：** Manifest 短 TTL（要能追加 HEVC/AV1 档）；`.m4s` 按 content-hash 寻址，不可变、永久缓存。
3. **Connection warming：** 与 Edge POP 保持 HTTP/3 连接池，去掉握手 RTT。200ms 预算里一次 TLS 握手就会超。

---

## 5. 跨团队落地（面试里主动说的边界）

| 契约 | Owner | 消费者 |
|------|-------|--------|
| 分块协议（GOP 对齐、checksum、session TTL） | Client + Ingest | 转码 Splitter |
| Fast-path 档位矩阵（哪些 SKU 必须先出 720p） | 编码 / 容量规划 | Orchestrator |
| Manifest schema（可追加 variant，禁止改已发布分片 hash） | Playback | CDN、Client |
| 审核 SLA（block vs 延迟发布） | Trust & Safety | DAG：审核未过不能标 Ready |
| 存储 TTL 与删除证明 | Blob + Privacy | 法务 / GDPR |

**收口金句：** 客户端、边缘、GPU 集群、存储、CDN 五支队伍，唯一的共享产物是 **video_id + immutable segment hash + 可追加的 Manifest**。谁改已发布分片的内容，缓存就永久错。

---

## 6. 数字预算（当场算并收口）

### 写入

$$
100 \times 10^{6}\ \mathrm{videos/day} \times 30\ \mathrm{MB} = 3\ \mathrm{PB/day}
$$

$$
3\ \mathrm{PB/day} \times 8 / 86400 \approx 280\ \mathrm{Gbps\ average\ ingest}
$$

峰值按 4× 日均 → **约 1 Tbps ingest**，必须铺在全球 Edge POP 上，不能打一台 Origin。

转码后 50 MB/video → **5 PB/day** 新增。Hot 留 48h、3 副本：

$$
5\ \mathrm{PB/day} \times 2\ \mathrm{days} \times 3 \approx 30\ \mathrm{PB\ hot}
$$

Cold 走 8+4，相对 3 副本省一半磁盘。年增量若全留 ≈ 1.8 EB，所以 **TTL + 低播放下沉是存储能活下来的原因**，不是买盘。

### 回放

日 50 亿次播放。按每次均服务 8 MB（ABR + 完播率不满）：

$$
5 \times 10^{9} \times 8\ \mathrm{MB} = 40\ \mathrm{PB/day}
$$

$$
40\ \mathrm{PB/day} \times 8 / 86400 \approx 3.7\ \mathrm{Tbps\ average}
$$

晚高峰 5–10× → **20–40 Tbps**。这就是为什么 Slow path 的 AV1 值回编码电费：CDN 账单按 Tbps 计。

### 3 秒发布

60s / 4s GOP ≈ 15 段。单段 H.264 720p 在 NVENC 上通常 < 1s（常快于实时）。墙钟 ≈ `split + max(segment_transcode) + assemble` ≈ 0.2 + 1.5 + 0.3 = **2.0s**，P95 余量给排队和审核。

**如果 GPU 队列超过 1s，3s SLA 直接破。** 所以 Fast path 必须有独立高优先级队列，不能和 AV1 批处理抢卡。

### 200ms TTFF（未命中预加载）

| 项 | 预算 |
|----|------|
| 已有 HTTP/3 连接 | 0 |
| Manifest（边缘命中） | 20–40 ms |
| 首个 GOP（边缘命中） | 40–80 ms |
| 解封装 + 首帧解码 | 30–50 ms |
| 余量 | ~50 ms |

任一层回源到 HDD 卷，这一行就会变成几百毫秒。所以 L1/L2 命中率是 TTFF 的真实 SLO，不是播放器代码。

---

## 7. 关键决策与被否方案

| 决策 | 我选 | 否掉的 | 为什么 | 什么会让我翻盘 |
|------|------|--------|--------|----------------|
| 上传协议 | **QUIC + GOP/2–4MB 分块直传 Edge** | 整文件经 API 机 | 3 PB/day 不能经过业务机；TCP HOL 在弱网会杀死续传 | 企业网把 UDP 墙掉，降级 HTTP/2 多流 |
| 切分单位 | **I-Frame / GOP 边界** | 固定 4MB 无视帧 | 非关键帧切开后每段都要重编依赖，并行失去意义 | 客户端来的是不可重切的封闭 GOP，只能整段转 |
| 转码并行 | **按 GOP 散到 GPU 池** | 单机转完整视频 | 唯一能把 60s 收进 3s 的办法 | 视频 < 8s，切分开销 > 收益，整段转更快 |
| 编码阶梯 | **Fast H.264 720p + 异步 HEVC/AV1** | 一次出齐所有档 | 3s SLA 和带宽账单冲突，必须拆成两条路径 | 产能永远过剩，可以同步出 AV1 |
| 小文件存储 | **Haystack 大 Volume + 内存索引** | 一切片一对象纯 S3 | inode / 寻道 / PUT QPS 都会先爆 | 对象存储的小文件聚合已经够好，且运维成本更低 |
| 热数据冗余 | **3 副本** | 一上来就 EC | 热路径要重建速度和尾延迟，EC 重建会拖 TTFF | 热集小到 NVMe 全缓存，Origin 用 EC 也行 |
| Manifest | **短 TTL，分片不可变 hash** | 整包一起缓存 | Slow path 追加高清档时不能让 CDN 吐旧清单、旧分片 | 无。分片可变是缓存毒化 |

**关于 GOP 对齐，单独讲一句：** 转码并行的正确性取决于切点是 IDR。切在 P/B 上，decoder 无法独立起播该段，ABR 切换也会花屏。这是编解码知识和分布式调度的交汇点，面试官在等这句。

---

## 8. 失效模式与降级

| 潜在瓶颈 / 故障 | 检测 | 应对 | 用户可见 |
| --- | --- | --- | --- |
| **GPU 队列积压** | Fast-path 排队 > 500ms | 只出 H.264 720p；HEVC/AV1 进低优先级 Batch | 发布仍 < 3s，后续画质慢慢变好 |
| **Viral 缓存击穿** | 单 key QPS 尖峰 / Origin 带宽 | Edge **Singleflight / request collapsing**；主动推全球 POP | 首批用户可能多 100ms，随后命中 |
| **弱网上传中断** | chunk ACK 超时 | 细粒度重传 + 持久化 offsets；session 续期 | Post 按钮转圈，不丢已传块 |
| **存储机架故障** | volume checksum / 副本落后 | Cross-rack / Cross-AZ；后台 repair | 无（3 副本）或短暂 404（修复中禁播该 variant） |
| **审核超时** | DAG 节点 SLA | Fast path 可先对粉丝可见但带 pending 标记，或延迟 Ready——**产品决策，开场确认** | 延迟出现在 Feed |
| **Manifest / 分片不一致** | packager 校验每段 hash | 不发布 Ready；重跑 Assembler | 无（失败在上线前） |
| **Ingest Buffer 写满** | 水位 | 拒绝新 session（429）保护已有 Commit；扩容 Buffer | 新上传失败，已点 Post 的不受影响 |
| **CDN 配置把分片设成短 TTL** | 回源比异常升高 | 告警 + 强制 hash 不可变；回滚配置 | TTFF 变差、账单爆炸 |

**一条设计原则：** Slow path、推荐特征、非阻断型审核都是**可失败的下游**。它们不许反压 Fast path 的 Ready 信号。实现上：独立队列 + 独立配额 + Ready 只依赖 Fast ladder +（产品选择的）审核门闩。

---

## 9. 怎么证明它是对的

### 9.1 金标回放与差分

转码必须能 **bit-exact / checksum 复现**：同一 GOP + 同一 encoder binary + 同一 preset → 相同 hash。否则"更新编码器省 5% 码率"无法回归。语料：固定 200 条短视频覆盖运动/暗光/字幕/竖屏。

### 9.2 台架

| 测什么 | 怎么测 |
|--------|--------|
| P95 commit→ready | 合成 1 亿/day 的缩小流量（1/1000）打预发 DAG，看排队而非只看单视频 |
| TTFF | 冷连接 vs 热连接 vs 预加载命中，分三档报 P50/P90 |
| 缓存击穿 | 单 video_id 从 0 打到 100k QPS，验证 collapsing 后 Origin QPS ≈ 1/POP |
| 续传 | 随机 drop 30% chunk，校验只重传失败块、Commit 后文件完整 |
| 删除 / TTL | Stories 到期后 CDN + 索引 + volume 标记三重不命中 |

### 9.3 Fleet SLI

| SLI | 阈值示例 |
|-----|---------|
| Commit → Ready P95 | < 3s |
| Fast-path 排队 P95 | < 500 ms |
| TTFF P90（无预加载） | < 200 ms |
| 预加载命中率 | > 70% 上滑起播 |
| L1 分片命中率（热 48h） | > 95% |
| 上传最终成功（含续传） | > 99.5% |
| 数据丢失 / 无法播 | ~0 |
| Origin 被热点打穿次数 | 0 / 周 |

**灰度：** encoder preset、CDN TTL、Fast-path 档位都按 video_id 哈希放量。任一组 TTFF 或 Ready 延迟回归即 halt。

---

## 10. 演进与组织

### 分阶段

| 阶段 | 出口标准 |
|------|----------|
| P0 | 直传 + 单档 H.264 + 对象存储能播；SLA 先放宽到 15s |
| P1 | GOP 并行 + Fast path P95 < 3s；QUIC 续传 |
| P2 | Haystack 聚合 + L2 shield；热点 collapsing |
| P3 | Slow path AV1；EC 冷热分层；Stories TTL 删除证明 |
| P4 | 预加载与 Manifest 动态追加成为默认客户端能力 |

P0 就上 Fast/Slow 拆分的**接口**（Manifest 可追加 variant），否则 P3 要迁一遍播放器。

### 组织

容量规划 own GPU 配额和 CDN 账单；Client own 预加载与 QUIC；Media infra own DAG 与存储。事故复盘默认问三句：是排队、是回源、还是切错 GOP。

**收口金句（E6）：** 这不是一个"视频上传服务"，是一条 **Commit 同步、处理异步、分片不可变、清单可追加** 的流水线。3 秒靠 GOP 级并行的 Fast path；200ms 靠边缘命中和连接预热；PB 级磁盘靠小文件聚合成大 Volume，再按热度从 3 副本掉到纠删码。

---

## 面试表达建议

- **突出硬件/编解码与分布式的交汇：** GOP 边界对齐、AV1 vs HEVC 的算力–带宽权衡、QUIC Connection Migration。
- **主动画出数据流与控制流：** 白板上标明哪些同步阻塞（Commit、可选的审核门闩），哪些是解耦 DAG（转码、鉴黄、高清压制）。
- **被问"为什么不直接 S3 + Elastic Transcoder"：** 吞吐和 TTFF 都过得去的玩具规模可以；1 亿条/天时 PUT QPS、小文件寻道、单视频墙钟转码、热点回源这四项会同时爆。上面每一层都是在拆其中一项。
