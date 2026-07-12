# 02 - 多租户隔离与安全代码执行沙箱

> **完整标准答案（Docker vs MicroVM、100ms Warm Pool/Snapshot、cgroups、default-deny）：**  
> [isolated-code-sandbox.md](./isolated-code-sandbox.md) · Demo：[sandbox_orchestrator.py](./sandbox_orchestrator.py)

> **场景：** Advanced Data Analysis、Code Interpreter、Tool Calling 执行用户/模型生成的代码

---

## Q4. 设计多租户、安全的云端/端侧代码执行沙箱

### 4.1 威胁模型

| 威胁 | 示例 |
|------|------|
| 文件系统逃逸 | 读 `/etc/passwd`、写 host 路径 |
| 网络滥用 | 扫描内网、DDoS 反射、挖矿 |
| 资源耗尽 | 死循环、fork bomb、内存炸弹 |
| 侧信道 | 同 host 租户 timing 攻击 |

### 4.2 隔离技术选型

| 技术 | 隔离级别 | 冷启动 | 面试结论 |
|------|----------|--------|----------|
| **进程 + seccomp** | 弱（共享内核） | 快 | 仅可信代码 |
| **Docker 容器** | 命名空间隔离，**共享内核** | ~秒级 | 多租户不够安全 |
| **MicroVM (Firecracker, Kata)** | **硬件虚拟化**，独立内核 | ~125ms（可优化） | ✅ 多租户首选 |
| **WASM (Wasmtime)** | 沙箱字节码，无 syscalls 默认 | 毫秒级 | 受限语言/库 |

> 面试金句：Docker 防君子不防内核漏洞；生产多租户执行不可信代码用 **MicroVM**，配合最小 syscall allowlist。

### 4.3 架构

```
API Gateway
    │
    ▼
Sandbox Orchestrator
    ├─ Warm Pool (pre-boot MicroVMs)
    ├─ Snapshot Restore (mem snapshot → 100ms 内就绪)
    └─ Job Queue (priority per tenant)
           │
           ▼
    MicroVM Worker
    ├─ read-only rootfs (overlay)
    ├─ ephemeral rw /tmp (size cap)
    ├─ cgroups: CPU quota, memory max, pids max
    ├─ network: default-deny egress, allowlist API proxy
    └─ timeout watchdog (SIGKILL at T+30s)
           │
           ▼
    Result Object Store (S3) ──► signed URL to client
```

### 4.4 冷启动优化（100ms 目标）

1. **Warm Pool** — 维持 N 个已 boot 的空 MicroVM，`FIFO` 分配
2. **Snapshot / Clone** — Firecracker snapshot 恢复 vs 完整 boot
3. **语言 Runtime 预装** — Python 解释器 + 常用 libs  baked in image
4. **Over-provision** — 按租户历史峰值预测 pool 大小

### 4.5 cgroups v2 资源限制

```bash
# 示例：限制 1 CPU，512MB 内存，禁止 swap
echo "+cpu +memory +pids" > /sys/fs/cgroup/sandbox/cgroup.subtree_control
echo "100000 100000" > .../cpu.max          # 1 core
echo "536870912"   > .../memory.max         # 512MB
echo "64"          > .../pids.max
```

**网络 default-deny：**

- MicroVM 无公网 IP；出站经 **egress proxy**（审计 URL、限流）
- 内网 metadata service (169.254.169.254) 必须 block

### 4.6 端侧沙箱（Embedded Experiences）

| 云端 | 端侧 |
|------|------|
| Firecracker / Kata | **SELinux + seccomp + WASM** 或 OS 级 App Sandbox |
| 100ms 冷启动 | 预加载 interpreter；无 VM 时用 **Capability-based** 限制 |

### 4.7 Failure Modes（必答）

| 失效 | 现象 | 缓解 |
|------|------|------|
| Pool 耗尽 | 用户等待 | 队列 + 扩容 + 降级「稍后重试」 |
| OOM kill | 作业失败 | 返回结构化 error；不 crash orchestrator |
| 内核 CVE | 租户逃逸 | MicroVM + 快速 patch 滚动 |
| 慢作业占满 pool | 全局阻塞 |  per-tenant 并发上限 + 作业超时 |

### 4.8 10x Scaling Drill

> 「1K 并发执行时 warm pool 够用；100K 时 **Orchestrator 调度与 snapshot 存储 IO** 成瓶颈。我会：分 region pool、tenant 隔离 quota、S3 结果异步上传、失败作业 dead-letter queue。」

---

## 关联

- [docs/17-AWS-EC2-Nitro-系统设计.md](../docs/17-AWS-EC2-Nitro-系统设计.md) — Nitro 虚拟化、Firecracker 背景
- [amazon_cpp/docs/07-Linux系统与设计题.md](../amazon_cpp/docs/07-Linux系统与设计题.md) — 线程池、队列
