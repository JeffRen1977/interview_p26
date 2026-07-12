# 设计多租户、安全的云端/端侧代码执行沙箱

> **场景：** Advanced Data Analysis / Code Interpreter — 运行 **LLM 生成的不可信 Python/C++**  
> **核心考点：** 隔离边界、100ms 冷启动、cgroups + default-deny 网络  
> **关联 Demo：** [sandbox_orchestrator.py](./sandbox_orchestrator.py)

---

## 0. 澄清需求（Ambiguity）

| 维度 | 必问 / 假设 | 设计影响 |
|------|-------------|----------|
| 信任模型 | 代码来自模型 + 用户，**完全不可信** | 必须硬件级隔离，不能只靠 Docker |
| SLA | 点击「Run」到可执行 **P99 < 100ms** 就绪 | Warm pool + snapshot |
| 语言 | Python 数据科学生态 vs 任意二进制 | Image 预装 vs 编译工具链 |
| 网络 | 是否允许 pip / 外呼 API？ | Default-deny + allowlist proxy |
| 多租户 | 同 host 密度 vs 强隔离 | MicroVM 密度 vs 安全权衡 |
| 端侧 | 设备无 KVM？ | WASM / OS sandbox 降级路径 |

**开场金句：**

> 这不是「起个 Docker 跑一下」题。多租户执行不可信代码的第一性原理是：**攻击面在共享内核**。Docker 共享 host kernel；MicroVM 有独立 guest kernel + 硬件虚拟化。目标是在 **100ms** 内给出隔离执行环境，并用 cgroups / 网络策略把爆炸半径锁死。

---

## 1. 威胁模型

| 威胁 | 攻击示例 | 若只用 Docker |
|------|----------|---------------|
| **内核逃逸** | 容器逃逸 CVE、错误 capability | 共享内核 → 直接威胁 host / 邻租户 |
| 文件系统 | 读 host 密钥、写 `/proc` | namespace 可配错；仍依赖内核正确性 |
| 网络 | SSRF、扫内网、挖矿、**DDoS 跳板** | 默认桥接网络过于宽松 |
| DoS | fork bomb、死循环、内存炸弹 | 需 cgroups；逃逸后仍可打 host |
| 侧信道 | 同 CPU 缓存计时 | 多租户共 host 固有风险；MicroVM 更好但仍非零 |
| 供应链 | 恶意 `import` / 动态下载 | 只读 rootfs + 禁出站 |

---

## 2. 隔离技术选型（必 Deep Dive）

### 2.1 对比表

| 技术 | 隔离边界 | 内核 | 冷启动 | 密度 | 多租户不可信代码 |
|------|----------|------|--------|------|------------------|
| 进程 + seccomp | 弱 | 共享 | 最快 | 最高 | ❌ |
| **Docker / runc** | namespaces + cgroups | **共享 host kernel** | ~百 ms–数 s | 高 | ❌ 不够 |
| gVisor | 用户态内核拦截 syscall | 大部分不进 host | 中 | 中高 | 可作增强，非银弹 |
| **Kata Containers** | 轻量 VM + 容器体验 | **Guest kernel** | 优化后亚秒 | 中 | ✅ |
| **Firecracker MicroVM** | 最小虚拟机 | **Guest kernel** | **快照可 ~125ms→&lt;100ms** | 中高 | ✅ **首选** |
| WASM (Wasmtime) | Capability / 无默认 syscall | 不跑原生内核代码 | 毫秒 | 很高 | ✅ 但库生态受限 |

### 2.2 为什么 Docker「不够安全」？

```
App → libc → syscalls → ★ Host Kernel ★ → 硬件
         Docker 只在这里加了 namespace / cgroups 围栏
```

- 容器 **不是** 虚拟机；隔离依赖 host 内核的正确性与配置。
- 历史容器逃逸、过度 `privileged`、错误挂载 Docker socket，都是共享内核的后果。
- **面试结论：** Docker 适合可信微服务；**不可信多租户代码执行 → MicroVM**。

### 2.3 为什么 MicroVM（Firecracker / Kata）？

```
App → Guest Kernel → Virtualization (KVM) → Host Kernel → 硬件
         ▲
   租户即使拿下 guest，仍隔着 hypervisor；host 攻击面极小（Firecracker 刻意砍设备模型）
```

- **硬件虚拟化（VT-x/AMD-V）+ 独立 guest OS**
- Firecracker：为 serverless 设计，设备模型极简 → 攻击面小、启动快
- Kata：用 VM 跑「看起来像容器」的工作负载，兼容 K8s 生态

### 2.4 WASM 放在哪？

- 端侧 / 受限插件：毫秒级、默认无文件系统无网络  
- 云端「完整 pandas / native C++」：仍需 MicroVM（或把原生扩展放进预置镜像）

---

## 3. 系统架构

```
Client / ChatGPT Tool Call
    │
    ▼
API Gateway (Auth · RPM · 代码大小限制)
    │
    ▼
Sandbox Orchestrator
    ├─ Job Queue (per-tenant concurrency cap)
    ├─ Warm Pool Manager (ready MicroVMs)
    ├─ Snapshot Store (mem/disk snapshots)
    └─ Policy Engine (timeout, network allowlist, cgroup profile)
           │
           │ assign / restore
           ▼
    ┌──────────────────────────────────────────┐
    │ MicroVM Instance                         │
    │  · RO rootfs (Python + libs baked-in)    │
    │  · RW ephemeral disk (size-capped /tmp)  │
    │  · vsock / virtio for stdin/stdout/files │
    │  · cgroup: cpu / memory / pids           │
    │  · net: no egress OR proxy allowlist     │
    │  · watchdog: hard kill @ T+timeout       │
    └──────────────────┬───────────────────────┘
                       │ artifacts
                       ▼
              Object Store (S3) → signed URL
```

**原则：** Orchestrator 永不在同进程执行用户代码；只调度与回收。

---

## 4. 冷启动 &lt; 100ms（Warm Pool + Snapshot）

完整 guest boot（内核 + systemd + Python import）通常 **远超 100ms**。达标靠 **预热与快照**，不是「优化 Dockerfile」。

### 4.1 延迟分解（面试白板）

| 阶段 | 冷 boot | Snapshot restore | Warm pool 取出 |
|------|---------|------------------|----------------|
| 分配 VM 槽位 | 5–20ms | 5–20ms | ~1ms |
| 加载/恢复内存 | 内核启动数百 ms+ | **快照恢复主导** | 已在内存 |
| Runtime ready | import 重 | 已预热在快照里 | 已 ready |
| **合计目标** | 秒级 | **~50–125ms** | **&lt;10–50ms** |

### 4.2 Warm Pool

```
维持 N 个状态 = READY 的 MicroVM（Python 已 import 常用库）
用户 Run → pop 一台 → 注入代码 → 执行
执行完 → 销毁（推荐）或 经消毒后回池（风险更高）
异步补池到水位线 N
```

- **FIFO / 按镜像版本分池**（pandas 镜像 vs 裸 Python）
- **Over-provision：** 按历史 QPS × P99 执行时间估 N  
- Pool 耗尽：排队 + 503；或降级到「稍慢的 snapshot restore」

### 4.3 Snapshot / Clone（Firecracker）

1. 手工 boot 一台「黄金」VM：装好解释器、预 import、停在 agent 等待注入  
2. **Pause + Snapshot**（内存 + 设备状态）写入存储  
3. 请求到来：**Restore snapshot → resume** → 注入 job  

要点：

- Snapshot 放 **本地 NVMe**（别每次拉远程）  
- 多版本 snapshot（镜像更新蓝绿）  
- Restore 后做 **entropy / 机器身份** 刷新，避免克隆指纹碰撞  

### 4.4 达到 100ms 的组合拳

```
默认路径: Warm Pool hit          → 几十 ms 内开始跑
池空:     Local Snapshot restore → 瞄准 <100ms P99
灾难:     Cold boot              → 仅扩容/发布时，不走用户关键路径
```

---

## 5. 资源限制：cgroups + 网络 Default-Deny

### 5.1 cgroups v2（防死循环 / fork bomb / OOM 邻居）

```bash
# 每个 MicroVM / 作业一个 cgroup
echo "+cpu +memory +pids +io" >> cgroup.subtree_control

# CPU：最多 1 核
echo "100000 100000" > cpu.max

# Memory：512MiB hard limit（触发 OOM killer 在 guest/作业内）
echo "536870912" > memory.max
echo "536870912" > memory.swap.max   # 或 0 禁 swap

# PIDs：防 fork bomb
echo "64" > pids.max

# 磁盘 IO 限速（可选）
echo "8:0 rbps=10485760 wbps=10485760" > io.max
```

| 资源 | 限制意图 |
|------|----------|
| CPU | 死循环不能占满 host |
| Memory | 内存炸弹；保护邻租户 |
| pids | fork bomb |
| 磁盘配额 | `/tmp` 写满攻击 |
| **wall clock timeout** | cgroup 不管「逻辑时间」→ 必须有 **watchdog SIGKILL** |

> cgroups **不是** 隔离边界，是 **资源配额**。隔离靠 MicroVM；配额靠 cgroups（host 侧套在 VM 进程上）。

### 5.2 网络：Default-Deny Egress

**默认拒绝一切出站**，需要时才开洞：

```
MicroVM
  │  (无公网路由 / 安全组 deny all)
  ▼
Egress Proxy (可选)
  ├─ allowlist: pypi 镜像 / 租户配置的 API 域名
  ├─ 审计日志 + 速率限制（防 DDoS 跳板）
  └─ 阻断: 169.254.169.254、10.0.0.0/8、内网 metadata
```

| 模式 | 行为 |
|------|------|
| **Lockdown（默认）** | 无网；数据只能通过 vsock 交回 Orchestrator |
| Allowlist | 仅 HTTPS 到批准域名 |
| 用户显式授权 | UI 确认后临时开网（仍经 proxy） |

**防 DDoS 跳板：** 即使开网，也对连接数 / 带宽 / PPS 做 cgroup + proxy 限速。

### 5.3 文件系统与 Syscall

- Rootfs **只读**；可写层容量封顶；禁止 Docker socket / host path 挂载  
- seccomp / 关危险 capability（即使在 guest 内也防御纵深）  
- 禁止 `ptrace`、原始套接字（按策略）

---

## 6. 作业生命周期

```
1. Submit(code, tenant, limits, network_policy)
2. Admit: per-tenant 并发 & 日配额（可复用 RPM 思想）
3. Acquire VM: warm pool | snapshot | (rare) cold
4. Inject: 写 /workspace/main.py，传 stdin 文件
5. Exec: timeout=30s；stdout/stderr 环形缓冲上限
6. Collect: 产物 → S3；返回 exit_code + logs + artifact URLs
7. Destroy VM（一作业一毁，防状态残留）
8. Replenish pool
```

**一作业一毁** 优于「洗白回池」：残留恶意 cron/内核模块风险更低。

---

## 7. 端侧路径（Embedded Experiences）

| 约束 | 方案 |
|------|------|
| 无 KVM | **WASM** / 平台 App Sandbox / seccomp-bpf |
| 要原生加速 | 厂商 TEE（若有）或把危险工作丢云端 MicroVM |
| 冷启动 | 预加载解释器；常驻 daemon；禁止每次拉镜像 |

话术：端侧用 capability 沙箱做「尽力隔离」；**强安全多租户仍以云端 MicroVM 为准。**

---

## 8. Failure Modes

| 故障 | 用户现象 | 处理 |
|------|----------|------|
| Warm pool 空 | 变慢或排队 | 扩池；snapshot 回退；限流 |
| OOM / kill | 作业失败 | 结构化错误；不计成功；不崩 Orchestrator |
| Snapshot 损坏 | restore 失败 | 校验 checksum；冷池备用 |
| 内核 CVE | 潜在逃逸 | 滚动 patch guest+host；缩短 VM 寿命 |
| 慢作业占坑 | 池枯竭 | 硬 timeout + per-tenant 并发帽 |

---

## 9. 10x / 100x Scaling Drill

| 规模 | 最先崩 | 动作 |
|------|--------|------|
| 1× | 单机 pool | 多 AZ warm pool |
| 10× | Snapshot IO / Orchestrator | 本地 NVMe；分片调度；按镜像版本分池 |
| 100× | 密度与噪声邻居 | 提高销毁率；大客户独享 host；排队 + 优雅降级 |

> 「100x 时不要为了密度退回纯 Docker。安全边界不能卖；用快照与池化换延迟，用配额换公平。」

---

## 10. 面试口述 3 分钟版

1. **威胁：** 不可信代码 → 共享内核不可接受。  
2. **选型：** Docker 共享 host kernel；**Firecracker/Kata = 硬件隔离**。  
3. **100ms：** Warm pool 为主，Snapshot restore 托底，冷 boot 不走用户路径。  
4. **配额：** cgroups 限 CPU/内存/pids + **墙钟 timeout**；网络 **default-deny**，出站只经 allowlist proxy。  
5. **多租户：** 一作业一毁；per-tenant 并发；结果走对象存储。

---

## 11. 参考实现

```bash
cd openai
python3 sandbox_orchestrator.py
```

进程内模拟：Warm Pool、Snapshot restore 计时、cgroup 式限额、default-deny egress allowlist、超时杀死——**非真实 Hypervisor**，用于白板对照。

---

## 12. 追问清单（自测）

- [ ] Docker 与 MicroVM 的内核关系一句话怎么说？  
- [ ] 为什么 cgroups 不能替代虚拟化？  
- [ ] 100ms 是靠优化 boot 还是靠 pool/snapshot？  
- [ ] 执行完回池还是销毁？为什么？  
- [ ] Default-deny 下用户要 `pip install` 怎么办？  
- [ ] 如何防止沙箱变成 DDoS 肉鸡？  
- [ ] 端侧没有 KVM 时你的降级方案？
