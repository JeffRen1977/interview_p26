# ISP 节点图拓扑排序与 Buffer 生命周期

你的清单里写了「Topological sort of an ISP node DAG」但没有实现。这一篇补上，并且加了 **peak buffer 分析**——这是把 CS-101 题变成相机题的那一步。

实现与自测：[`isp_graph_topo.cpp`](./isp_graph_topo.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 isp_graph_topo.cpp -o isp_graph_topo && ./isp_graph_topo
```

---

## 1. 面试官不会说「拓扑排序」

他会说：「用户配了一个 usecase，CamX/Chi 要把这张节点图编译成执行顺序。写一下。」典型图：

```
sensor → IFE ─┬→ stats → 3A
              └→ BPS → IPE ─┬→ display
                            └→ encoder
```

## 2. Kahn 算法，用最小堆保证确定性

```c
入度为 0 的进堆 → 弹出 → 输出 → 后继入度 -1 → 归零则进堆
```

用 `priority_queue` 而不是普通 `queue`：**同一张图必须永远编译出同一个调度**。相机 pipeline 每次 build 顺序都不一样是调试灾难。

**环检测是免费的**：输出少于 N 个节点就说明有环。

## 3. 但「有环」不是能用的报错

调优工程师把 IPE 接回 BPS 时，你必须**打印出那个环**。DFS 三色（白/灰/黑），碰到灰色节点就沿 parent 链回溯：

```c
for (int x = v; x != w; x = parent[x]) cycle.push_back(x);
cycle.push_back(w);
```

**迭代实现不要递归**——500 个节点的图不该有爆栈风险。自环（TNR 读自己上一帧的输出）也要能报出来。

**菱形不是环**：一个生产者两个消费者再汇合，是完全合法的 pipeline（预览 + 编码共用 IPE 输出）。测试里专门验了这一点。

## 4. Buffer 峰值：这才是相机题

给定调度顺序，一个节点的输出 buffer 在**所有消费者跑完之后**才能回收。走一遍顺序，按消费者数量做引用计数，记录高水位：

```c
++live;                             // v 分配输出
peak = max(peak, live);             // 此刻 v 的所有输入都还活着 —— 峰值在这里
for (u : v.inputs) if (--rem[u] == 0) --live;
```

峰值统计**必须在回收之前**做，写在后面就会低估一个。

结果：
- **链式** 0→1→2→3：峰值 2
- **菱形**（src 被两个消费者用）：峰值 3
- 无消费者的节点是 sink（encoder/display），buffer 交出图外，整帧都算活着

这个数字就是你拿去开内存预算会的数字。追问延伸：调整调度顺序可以降低峰值（同一张 DAG 有多个合法拓扑序，选峰值最小的那个是 NP-hard，实践上用启发式）。

## 5. 追问清单

- **多个合法顺序怎么选？** 关键路径优先（缩短端到端延迟）vs buffer 峰值最小（省内存）。眼镜上选后者。
- **并行执行：** 拓扑序只给了偏序。真正调度要按硬件资源分组——IFE/BPS/IPE 是不同的 HW block，可以流水线并行跑不同帧。
- **动态图：** HDR 开关、夜景模式会改变图结构。是每次重编译还是预编译多张图切换？（预编译，切换要在帧边界。）
- **和 `number_of_islands` / `course schedule` 的关系：** 同一套代码。面试碰到 LeetCode 原题时，主动说「我们用这个编译 ISP usecase graph，还要算 buffer 峰值」。
