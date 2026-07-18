# 设计边缘协同 Agent Runtime（弱网 / 断网 / 最终一致性）

> **核心考点：** 弱网不可用 WebSocket 强耦合 → **Outbox + QUIC**；状态用 **CRDT 最终一致**；关键决策用 **动态 Quorum**；崩溃用 **WAL 可重放**；离线用 **Graceful Degradation**  
> **场景：** 分布式机器人避障、多设备工厂监测、微电网调度 — 节点跨物理位置，RTT 可飙到秒级，断网可持续分钟～小时  
> **关联：** [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) · [hybrid-sensor-routing.md](./hybrid-sensor-routing.md) · [embedded-sensor-event-loop.md](./embedded-sensor-event-loop.md) · [isolated-code-sandbox.md](./isolated-code-sandbox.md)

---

## 0. 澄清需求（Ambiguity）

传统「持续稳定网络」（WebSocket / 高频 HTTP 轮询）的集中式或强耦合分布式架构，在边缘会直接崩溃。开场先钉物理边界：

| 维度 | 假设 | 设计影响 |
|------|------|----------|
| 网络 | RTT **50ms → 5000ms+**；完全断网可持续 **分钟～小时** | 禁止热路径依赖同步 RPC；必须 Outbox |
| 节点 | 每节点独立算力与本地存储 | **完全离线自主决策** 是硬需求 |
| 协同 | 多节点完成复杂任务（避障 / 监测 / 调度） | 区分「可最终一致」vs「必须强一致」状态 |
| 故障 | 断电、崩溃、分区、IP 漂移（Wi-Fi↔5G） | WAL 可重放；QUIC Connection Migration |
| 一致性目标 | **Eventual Consistency + Self-Healing** | CRDT 合并；多数派继续服务，少数派 Safe Mode |

**开场金句：**

> 边缘协同不是「把 Raft 搬到设备上」。题眼是：**在 Network Partition 下仍能局部自治，重连后自愈收敛，且物理执行器永不双脑。**

---

## 1. 总体架构（单节点四层）

```
       ┌────────────────────────────────────────────────────────┐
       │             分布式边缘协同 Agent 节点架构 (单个节点)     │
       └────────────────────────────────────────────────────────┘

         [ 物理网络 / P2P 传输 ]
                    │  (弱网 / 间歇性断连 / 自动重连)
                    ▼
         ┌──────────────────────────────────────────────────────┐
         │ 1. 弹性通信层 (Resilient Transport)                  │
         │    - 混合 P2P (QUIC / Thread / Wi-Fi Aware)          │
         │    - 带有退避重试 (Exponential Backoff) 的消息队列   │
         └──────────────────┬───────────────────────────────────┘
                            │
                            ▼
         ┌──────────────────────────────────────────────────────┐
         │ 2. 状态同步与共识层 (State Sync & Consistency)       │
         │    - CRDT 用于松耦合协同                             │
         │    - 动态 Quorum 用于强一致性决策                    │
         └──────────────────┬───────────────────────────────────┘
                            │
                            ▼
         ┌──────────────────────────────────────────────────────┐
         │ 3. 本地持久化与日志层 (Local Durable Ledger)          │
         │    - Append-Only WAL                                 │
         │    - 嵌入式 KV (RocksDB / SQLite)                    │
         └──────────────────┬───────────────────────────────────┘
                            │
                            ▼
         ┌──────────────────────────────────────────────────────┐
         │ 4. 本地自主决策 Agent 引擎 (Autonomous Agent Core)   │
         │    - 本地 LLM (INT4) / 确定性状态机                    │
         │    - Graceful Degradation Mode                       │
         └──────────────────────────────────────────────────────┘
```

与 [smart-glasses-ai-runtime.md](./smart-glasses-ai-runtime.md) 的关系：眼镜题是 **单机功耗 + 端云路由**；本篇是 **多机弱网协同 + 一致性分层**。

---

## 2. 弹性通信层：QUIC + Outbox

### 2.1 为什么不是 TCP / 裸 WebSocket

| 问题 | TCP / 长连接 | QUIC |
|------|--------------|------|
| 高丢包（&gt;10%） | 拥塞控制 + 重传 → 延迟抖动 | UDP 基座，可调 FEC / 独立流 |
| IP 变化（Wi-Fi↔5G） | 连接断开，三次握手重来 | **Connection Migration** 保持会话 |
| 多传感器流 | 单流丢包 → Head-of-Line Blocking | **Multiplexing**：一流丢包不影响其它流 |

### 2.2 持久化 Outbox + 指数退避

所有待发消息（控制指令、状态增量）**不直接 send**，先写入本地 **Outbox Directory / 持久化队列**：

1. 断网期间：Exponential Backoff **with Jitter** 降频重连，省电、避免雷群。  
2. Link-state / Ping 探测恢复后：**批量排空** Outbox。  
3. 进程崩溃也不丢待发消息（与 §4 WAL 同源思想）。

> 面试金句：边缘通信的一等公民是 **Outbox**，不是 socket。socket 只是排空 Outbox 的管道。

---

## 3. 状态同步：CRDT + 动态 Quorum 混合

**绝对不能**在每次决策上跑「全员在线」的 Standard Raft——任一节点断开，整条产线停摆。

### 3.1 CRDT：松耦合、最终一致

适用于：节点坐标、传感器历史、设备活跃状态、局部意图等 **非不可逆物理控制**。

| 类型 | 用途 |
|------|------|
| LWW-Element-Set | 集合型状态；带逻辑时钟，后写覆盖 |
| PN-Counter | 可增减计数（电量调度、配额） |

机制要点：

- 每次更新附带 **Lamport Timestamp / Vector Clock**  
- 分区期间各自本地决策；重连只交换 **增量**  
- 数学上：**无中央服务器，本地 merge 即可收敛到同一状态**（与到达顺序无关）

### 3.2 动态 Raft / Quorum：强一致关键决策

适用于：主控选举、物理执行器权限分配等 **不可逆 / 互斥** 决策。

- 分区 **Local Raft Group**，而非全球一张大表。  
- **Dynamic Quorum：** 5 节点中 2 节点被隔绝 → $2 < \lceil 5/2 \rceil = 3$，少数派自动进入 **Safe Mode（只读）**，放弃决策权，避免 Split-Brain。  
- 多数派（3 节点）继续完整控制服务。

| 状态类别 | 一致性 | 机制 |
|----------|--------|------|
| 感知 / 意图 / 遥测 | 最终一致 | CRDT merge |
| 主控 / 执行器锁 | 强一致 | Local Raft + Dynamic Quorum |
| 物理动作 | 互斥 | **Lease（见追问 1）**，不用 LWW |

---

## 4. 本地持久化：WAL + Snapshot 可重放

Agent 必须是 **Replayable Durable Execution**：

1. **WAL：** 外部输入、协同决策、本地动作 **执行前** 先 Append-Only 落盘。  
2. **嵌入式 KV：** RocksDB（进程内、顺序写强、无网络开销）或 SQLite。  
3. **恢复：** 加载最近 **Snapshot** → 顺序重放后续 WAL → 毫秒级回到崩溃前协同状态。

> 与云端 Durable Workflow（Temporal 类）同构：边缘上把「编排器」压进单机 RocksDB + WAL。

---

## 5. 自主决策：Graceful Degradation 状态机

Agent 感知网络态，主动调「智商」与行为：

```
[ 网络良好 RTT < 100ms ] ──► Collaborative
                              • 共享全局状态
                              • 跨节点资源互助
                              • 可调云端复杂推理

                │ 网络恶化 / 丢包 / 间歇断开
                ▼

[ 间歇弱网 RTT > 2000ms ] ─► Local Group Only
                              • 降频邻居通信
                              • LWW-CRDT 合并
                              • 关键决策走本地多数派 Quorum

                │ 完全断网 / Partition
                ▼

[ Offline ] ───────────────► Autonomous Safe
                              • 停止跨节点协作请求
                              • 仅本地传感器闭环
                              • 硬编码 / 轻量规则防抱死
```

与眼镜 Hybrid Router 的呼应：都是 **按约束降级**；本篇降级维度是 **连通性与一致性强度**，而非单机功耗。

---

## 6. 面试官高频追问

### Q1：离线期间两节点对同一执行器发了冲突指令，重连后 LWW「后写赢」，但物理动作已做错？

**要点：** 这是 **物理冲突 ≠ 逻辑 CRDT 冲突**。

- 物理执行器控制 **禁止** 用无锁 LWW-CRDT。  
- 必须用 **Lease（租约）**：离线前向多数派申请独占租约（带 TTL）。  
- 持有者在 TTL 内唯一可动；未持有者即使离线，本地 Agent **拒绝** 对该执行器产生物理动作，进入安全等待。  
- 从物理根源杜绝双控；CRDT 只合并「意图/观测」，不合并「已执行的互斥动作」。

### Q2：网络恢复后积压消息冲垮脆弱带宽？

**要点：**

1. **Delta / 合并压缩：** Outbox 出队前合并。例：IMU 积压 50 次 → 只发最新状态，或卡尔曼压成一条曲线特征。  
2. **优先级 QoS：** Critical Control ≫ State Sync ≫ Telemetry/Logs；用 QUIC 流控限制低优先级带宽。  
3. 可加 **令牌桶 / 每邻居带宽预算**（对照 [llm-rate-limiter.md](./llm-rate-limiter.md) 的「按配额排空」思想）。

### Q3：为什么少数派必须 Safe Mode，而不是自己再选一个 Leader？

**要点：** 否则两个分区各选 Leader → Split-Brain → 双控执行器。少数派 **只读 + 本地安全规则**，等分区愈合再追赶日志。

### Q4：Vector Clock 和 Lamport 怎么选？

**要点：** Lamport 只给全序感、无法检测并发；需要「A∥B 并发后 merge」时用 **Vector Clock / 版本向量**，再配 CRDT 语义（LWW 则退化为比较时间戳）。

---

## 7. 面试口述 3 分钟版

1. **钉约束：** 分区常见、必须离线自治、区分最终一致 vs 强一致。  
2. **Transport：** QUIC + Connection Migration；一切经 **持久化 Outbox** + 退避重试。  
3. **一致性分层：** CRDT 管感知/意图；Local Raft + Dynamic Quorum 管主控；**Lease** 管物理执行器。  
4. **Durable：** WAL + Snapshot，崩溃可重放。  
5. **降级：** Collaborative → Local Group → Autonomous Safe。  
6. **追问预埋：** LWW 不能控执行器；恢复时 Delta + QoS 防洪峰。

---

## 8. 自测清单

- [ ] 为什么热路径不能依赖 WebSocket / 同步 Raft？  
- [ ] Outbox 解决了哪两类故障（断网、进程崩溃）？  
- [ ] 哪些状态用 CRDT？哪些必须 Quorum / Lease？  
- [ ] 5 节点丢 2 个时，少数派为何进入 Safe Mode？  
- [ ] 物理执行器冲突为何不能靠 LWW 解决？  
- [ ] 网络恢复防洪峰的两招是什么？  
- [ ] 与「智能眼镜单机 Hybrid」题的边界如何划分？
