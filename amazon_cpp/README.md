# Amazon C++ 面试备考（Senior / Principal SDE）

面向 **AWS、Annapurna Labs、Lab126、Robotics、Kuiper、Devices、Prime Video、Ads Infra** 等偏底层 C++ 岗位。

> Amazon C++ 面试比 Google 更偏 **语言本身和底层实现**。Senior/Staff 岗重点不是刷海量算法，而是能讲清 **为什么**。

## 文档目录

| 文档 | 覆盖 Part | 内容 |
|------|-----------|------|
| [01-对象模型与生命周期.md](./docs/01-对象模型与生命周期.md) | 1, 4, 5, 6, 7 | Rule of Five、Move、RAII、智能指针 |
| [02-内存指针与布局.md](./docs/02-内存指针与布局.md) | 2, 3, 9 | new/malloc、const 指针、对齐/padding |
| [03-虚函数与类型转换.md](./docs/03-虚函数与类型转换.md) | 8 | vtable/vptr、四种 cast |
| [04-STL底层原理.md](./docs/04-STL底层原理.md) | 10 | vector、unordered_map、map、deque、string SSO |
| [05-模板与异常.md](./docs/05-模板与异常.md) | 11, 12 | SFINAE、Concept、noexcept |
| [06-并发与内存模型.md](./docs/06-并发与内存模型.md) | 13, 14, 15 | mutex、atomic、memory_order、false sharing |
| [07-Linux系统与设计题.md](./docs/07-Linux系统与设计题.md) | 16, 17, 18 | 进程/线程、epoll、Thread Pool 等 |
| [08-高频问答速查.md](./docs/08-高频问答速查.md) | 全部 | 一页纸 FAQ |
| [09-无锁固定大小内存池.md](./docs/09-无锁固定大小内存池.md) | 13, 14, 15, 17 | Datapath 对象池、ABA、Placement New、Per-Core Cache |
| [10-memcpy与memmove.md](./docs/10-memcpy与memmove.md) | 2, 9, 15, 16 | 重叠检测、字对齐、SIMD/非临时写 |
| [11-令牌桶限流器.md](./docs/11-令牌桶限流器.md) | 15, 16, 17 | 惰性令牌桶、double 精度、RSS/Per-Core |

## 可运行示例

```bash
cd amazon_cpp
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

| 示例 | 对应考点 |
|------|----------|
| [examples/01_rule_of_five.cpp](./examples/01_rule_of_five.cpp) | Rule of Five、拷贝/移动、RVO |
| [examples/02_memory_placement.cpp](./examples/02_memory_placement.cpp) | new vs malloc、placement new |
| [examples/03_const_pointers.cpp](./examples/03_const_pointers.cpp) | const 修饰谁 |
| [examples/04_overload_resolution.cpp](./examples/04_overload_resolution.cpp) | lvalue/rvalue 重载解析 |
| [examples/05_move_forwarding.cpp](./examples/05_move_forwarding.cpp) | std::move、完美转发 |
| [examples/06_smart_pointers.cpp](./examples/06_smart_pointers.cpp) | unique/shared/weak、循环引用 |
| [examples/07_virtual_vtable.cpp](./examples/07_virtual_vtable.cpp) | 虚函数、vtable、虚析构 |
| [examples/08_memory_layout.cpp](./examples/08_memory_layout.cpp) | sizeof、padding、alignment |
| [examples/09_stl_internals.cpp](./examples/09_stl_internals.cpp) | vector 扩容、unordered_map |
| [examples/10_concurrency_atomic.cpp](./examples/10_concurrency_atomic.cpp) | mutex、CV、atomic、false sharing |
| [examples/11_lock_free_fixed_pool.cpp](./examples/11_lock_free_fixed_pool.cpp) | 固定内存、tagged CAS、无锁对象池 |
| [examples/12_memmove.cpp](./examples/12_memmove.cpp) | memmove 重叠、8 字节对齐拷贝 |
| [examples/13_token_bucket_rate_limiter.cpp](./examples/13_token_bucket_rate_limiter.cpp) | 惰性令牌桶、突发/精度/时钟回退 |

**系统设计手撕（C++ 实现）：** 见 [`../interview_handwrite/`](../interview_handwrite/)

## 准备优先级（Senior/Staff）

1. Modern C++ 核心：对象生命周期、Move、RAII、智能指针
2. STL 底层：vector、unordered_map、map、deque、string
3. 多线程：mutex、condition_variable、atomic、内存模型
4. 对象模型：虚函数、vtable、内存布局、类型转换
5. 内存管理：对齐、placement new、异常安全
6. Linux 基础：进程/线程、fd、epoll（系统岗）
7. LeetCode Medium + 设计题：见 [`../leetcode/`](../leetcode/)
8. Datapath 系统岗加餐：无锁内存池、ABA、Per-Core Cache、NUMA

## 18 Part 速查索引

| Part | 主题 | 文档 |
|------|------|------|
| 1 | 对象模型 | [01](./docs/01-对象模型与生命周期.md) |
| 2 | 内存管理 | [02](./docs/02-内存指针与布局.md) |
| 3 | 指针 | [02](./docs/02-内存指针与布局.md) |
| 4 | 引用 | [01](./docs/01-对象模型与生命周期.md) |
| 5 | Move Semantics | [01](./docs/01-对象模型与生命周期.md) |
| 6 | RAII | [01](./docs/01-对象模型与生命周期.md) |
| 7 | 智能指针 | [01](./docs/01-对象模型与生命周期.md) |
| 8 | Virtual | [03](./docs/03-虚函数与类型转换.md) |
| 9 | 内存布局 | [02](./docs/02-内存指针与布局.md) |
| 10 | STL | [04](./docs/04-STL底层原理.md) |
| 11 | Templates | [05](./docs/05-模板与异常.md) |
| 12 | 异常 | [05](./docs/05-模板与异常.md) |
| 13 | 并发 | [06](./docs/06-并发与内存模型.md) |
| 14 | Memory Model | [06](./docs/06-并发与内存模型.md) |
| 15 | Cache | [06](./docs/06-并发与内存模型.md) |
| 16 | Linux | [07](./docs/07-Linux系统与设计题.md) |
| 17 | 系统设计 C++ | [07](./docs/07-Linux系统与设计题.md) |
| 18 | Coding | [07](./docs/07-Linux系统与设计题.md) |
