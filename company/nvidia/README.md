# NVIDIA DRIVE / Autonomous Vehicles 面试备考

面向 **NVIDIA Autonomous Vehicles Platform**（DriveWorks SDK、传感器驱动、数据录制回放、车辆接口）及 **DRIVE** 生态相关软件岗。

> NVIDIA **按团队直招**（非 general pool），面试官通常是你未来的同事。面试重心不是 LeetCode 网页题，而是 **高性能系统设计、C++/嵌入式底层、AV 领域常识、One Team 文化**。

## 文档目录

| 文档 | 内容 |
|------|------|
| [01-系统设计.md](./01-系统设计.md) | 日志 ingestion、GPU job scheduler、实时 sensor fusion |
| [02-C++与嵌入式底层.md](./02-C++与嵌入式底层.md) | shared_ptr、vtable、volatile/const/inline、mutex/spinlock/semaphore |
| [03-AV领域与性能.md](./03-AV领域与性能.md) | Hybrid A*、INT8 量化部署、Amdahl 定律 |
| [04-行为面-One-Team.md](./04-行为面-One-Team.md) | 技术分歧、多线程 debug、Why NVIDIA DRIVE |

## 与本仓库其他资料的关系

| 主题 | 延伸阅读 |
|------|----------|
| 量化 / 端侧部署 | [docs/07-端侧部署题详解.md](../docs/07-端侧部署题详解.md) |
| 相机 / 传感器 / 延迟预算 | [docs/05-系统设计题与模拟面试.md](../docs/05-系统设计题与模拟面试.md) |
| C++ 手撕基础 | [docs/04-手撕代码指南.md](../docs/04-手撕代码指南.md) |
| 多传感器 / SLAM | [docs/10-MR感知题详解.md](../docs/10-MR感知题详解.md) |

## 四轮备考优先级

| 优先级 | 内容 |
|--------|------|
| **P0** | 3 道系统设计标准答案；shared_ptr + 同步原语；1 个 race condition STAR |
| **P1** | vtable / volatile；Hybrid A* 口述；INT8 量化流程 |
| **P2** | GPU scheduler 细节；Amdahl 手算；Why DRIVE 话术 |
| **P3** | LeetCode 图/堆（scheduler 题可能用到） |

## 面试流程（典型）

```
Recruiter → Phone Screen (C++ + 项目)
         → Virtual/Onsite Loop (2–4 轮)
              ├─ System Design (throughput / latency / hardware-aware)
              ├─ C++ / OS / Concurrency (deep internals)
              ├─ AV Domain (planning / perception deploy basics)
              └─ Behavioral (One Team, debug war stories)
         → HM / Team Match
```
