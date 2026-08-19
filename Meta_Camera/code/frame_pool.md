# 定长帧 Buffer 池与引用计数

你的 `coding_interview_guide.md` 把「定长块分配器 / 内存池」列为高频，但 `code/` 下没有可跑的题。这一篇补上，并且加了**引用计数**——因为真实相机里一帧要同时给 preview、encoder、CV 三个消费者。

实现与自测：[`frame_pool.cpp`](./frame_pool.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 -pthread frame_pool.cpp -o frame_pool && ./frame_pool
```

---

## 1. 为什么不能用 `malloc`

采集路径上 30–90 fps 调 `malloc(4MB)`：**延迟不确定**（可能触发 `mmap`/缺页）、**会碎片化**（长时间跑必然失败）、**不保证对齐**（DMA 要 64B 或页对齐）。所以相机驱动一律**启动时一次性分配，运行时只借还**。

## 2. 侵入式空闲链表

关键技巧：**空闲块的前 8 字节就是 next 指针**。元数据开销为 0，这也是它比位图法在大块场景下更优的原因。

```c
void* acquire() {                       void release(void* p) {
    if (!free_head_) return nullptr;        *(void**)p = free_head_;
    void* b = free_head_;                   free_head_ = p;
    free_head_ = *(void**)b;            }
    return b;
}
```

两个操作都是 **O(1)、无循环、无系统调用**。

- `block_size` 向上取整到 `alignment`，保证**每一块**都对齐，而不只是首块。
- `alignment` 必须是 2 的幂且 ≥ `sizeof(void*)`——否则 next 指针塞不进去。
- 池满返回 `nullptr`，**不是断言不是异常**。上层策略是丢帧（drop-oldest / drop-newest），不是崩溃。
- LIFO 归还是有意的：刚还回来的块还在 cache 里。
- `owns(p)` 检查地址落在 slab 内**且在块边界上**——能抓住「还了一个偏移指针」这种脏 bug。

## 3. 引用计数：为什么锁和原子要分开

```c
refs.fetch_sub(1, memory_order_release);   // 热路径：一次原子减
if (was_last) { fence(acquire); recycle(); }  // 只有最后一个付锁的代价
```

- **release** 保证消费者对 buffer 的写在回收前对所有人可见。
- 最后一次减之后补一个 **acquire fence**，让回收者看到前面所有消费者的写。
- 这是 `shared_ptr` 内部用的同一套 ordering，能讲清楚就是加分项。

`.cpp` 里有 8 线程同时 `release` 的测试，验证**只发生一次回收**。

## 4. 白板必答

- [ ] 空闲链表存在块内部，零元数据
- [ ] `block_size` 对齐取整；`alignment` 是 2 的幂
- [ ] 池满返回 `nullptr` + 丢帧策略
- [ ] 引用计数 `release` ordering + 最后一次 acquire fence
- [ ] 归还非本池指针要能检测

## 5. 追问清单

- **多线程 acquire 怎么办？** 这里用 mutex。无锁版本可以用 CAS 推链表头，但要处理 **ABA**（加 tag 计数器或用 hazard pointer）。老实说「无锁 free list 的 ABA 问题不值得在相机路径上冒险，mutex 的临界区只有两条指令」是很好的答案。
- **真实系统里 buffer 从哪来？** Linux 上是 **dma-buf**（`ion` / `dmabuf heaps`），Android 上是 **Gralloc**。池管的是 fd + 映射，不是 `malloc` 的内存。零拷贝的本质是把同一个 dma-buf fd 传给 ISP、NPU、encoder，配合 fence 同步。
- **cache coherency：** CPU 写完 buffer 给 DMA 读之前要 clean（flush）；DMA 写完给 CPU 读之前要 invalidate。漏一个就是「偶发花屏 / 撕裂」的经典根因。
- **不同尺寸怎么办？** 一个池只管一种尺寸。多尺寸就开多个池（slab allocator 思路），不要退回通用堆。
- 相关：[`c++/固定内存大小分配.md`](../../c++/固定内存大小分配.md)、[`c++/two_level_mempool.cpp`](../../c++/two_level_mempool.cpp)
