// Problem: In-Memory Video Ring Buffer (rolling pre-event clip)
//
 // Scenario (安防 / dashcam 经典题):
 //   Camera 持续往固定容量环形缓冲写帧。运动触发时，要把事件 *之前*
 //   最近 N 秒的画面上传，但不能对像素做 memcpy（帧往往是几 MB NV12）。
 //
 // 核心洞察：
 //   Ring 里存的不是像素本体，而是 shared_ptr<Frame>。
 //   snapshot() 只拷贝智能指针（refcount +1），零像素拷贝。
 //   Ring 覆写最老槽时，若 Upload 仍持有该 shared_ptr，帧不会被回收；
 //   两边都放开后，自定义 deleter 把 Frame 还回 FreeList，供 Recorder 复用。
 //
 // Whiteboard 五阶段（对着图讲）:
 //   1) Idle:    启动时预分配 Frame 进 FreeList（运行时零 new/delete）。
 //   2) Produce: Recorder acquire() → 填像素 → push 进 ring。
 //   3) Cache:   ring 满则覆写最老槽（shared_ptr 赋值，旧引用计数 -1）。
 //   4) Upload:  snapshot() 得到一段 oldest→newest 的 shared_ptr 向量。
 //   5) Recycle: Upload 释放 + ring 已不再持有 → deleter 归还 FreeList。
 //
 // Correctness 要点:
 //   - FreeList 用 shared_ptr + custom deleter；deleter 里持 weak_ptr，
 //     避免 FreeList ↔ shared_ptr 循环引用，也避免 pool 销毁后还 push。
 //   - snapshot() 返回时间序（oldest → newest），不是 slots_ 物理下标序。
 //   - 被覆写的帧只要 Upload 还握着 shared_ptr，就仍然合法可读。
 //
 // 面试追问:
 //   - 为何不用 unique_ptr？因为 ring 与 upload 需要共享所有权。
 //   - 为何 FreeList 要 enable_shared_from_this？deleter 需要安全地
 //     拿回 pool；weak_ptr::lock 失败说明 pool 已析构，此时直接 delete。
 //   - 多 Recorder / 多 Upload？本实现用一把 mutex 保护 ring；
 //     生产可拆成 SPSC ring + 无锁 FreeList，或 per-camera shard。
 //   - 真像素帧：Frame 里放 dma-buf fd / ION handle，而不是 vector<uint8_t>。

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

struct Frame {
    int id{0};
    // 真实系统这里是像素平面 / dma-buf fd / 时间戳 / 曝光参数等。
    // 白板用一个 id 就够演示所有权与回收路径。
};

// ---------------------------------------------------------------------------
// FreeList: 固定数量 Frame 对象池
// ---------------------------------------------------------------------------
//
// 为什么要池化？
//   录像热路径上 new/delete Frame（连带大块像素）会抖动延迟、打碎堆。
 //   启动时一次性分配 capacity 个 Frame，之后只在池里借还。
 //
 // 所有权模型:
 //   池内：unique_ptr<Frame>（独占，尚未借出）
 //   借出：shared_ptr<Frame> + 自定义 deleter
 //         deleter 负责「最后一个持有者释放时，把裸指针装回 unique_ptr 还池」
 //
 // weak_ptr 技巧:
 //   deleter 不能捕获 shared_ptr<FreeList>（会和池本身形成环，永远不析构）。
 //   捕获 weak_ptr：pool 还活着 → lock 成功 → 还池；
 //                 pool 已死   → lock 失败 → delete，避免悬空写。
 //
class FreeList : public std::enable_shared_from_this<FreeList> {
 public:
    explicit FreeList(int capacity) {
        // 预分配：之后 acquire/recycle 路径不再触碰堆分配器（Frame 本身）。
        for (int i = 0; i < capacity; ++i) {
            pool_.push(std::make_unique<Frame>());
        }
    }

    // 从池中借出一帧。池空返回 nullptr（调用方应 drop / backpressure）。
    std::shared_ptr<Frame> acquire() {
        std::unique_ptr<Frame> raw;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pool_.empty()) {
                return nullptr;
            }
            raw = std::move(pool_.front());
            pool_.pop();
        }
        // 锁外再构造 shared_ptr：自定义 deleter 可能较重，缩短临界区。

        // shared_from_this() 要求 FreeList 本身已由 shared_ptr 管理
        // （见 test 里 make_shared<FreeList>）。若栈上对象调用会抛 bad_weak_ptr。
        std::weak_ptr<FreeList> weak_self = shared_from_this();

        // release() 交出 unique_ptr 的所有权；此后生命周期由 shared_ptr 接管。
        // 注意：deleter 参数是裸 Frame*，必须最终要么还池要么 delete，禁止泄漏。
        return std::shared_ptr<Frame>(
            raw.release(),
            [weak_self](Frame* ptr) {
                // 先读业务字段：还池后别的线程可能立刻 reuse 并覆写 id。
                const int id = ptr->id;
                if (auto self = weak_self.lock()) {
                    // pool 仍在：把帧重新包装成 unique_ptr 塞回队列。
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    self->pool_.push(std::unique_ptr<Frame>(ptr));
                    std::cout << "  [FreeList] recycled frame #" << id << "\n";
                } else {
                    // FreeList 已析构（例如进程退出顺序异常）：不能再碰 pool_，
                    // 只能把这块内存真正归还给堆。
                    delete ptr;
                }
            });
    }

    // 当前空闲帧数。加锁是因为 acquire/recycle 并发改 queue。
    std::size_t free_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

 private:
    mutable std::mutex mutex_;
    std::queue<std::unique_ptr<Frame>> pool_;
};

// ---------------------------------------------------------------------------
// VideoRingBuffer: 固定容量、可覆写的时间窗缓存
// ---------------------------------------------------------------------------
//
 // 槽里放 shared_ptr<Frame>，不是 Frame 值：
 //   push 覆写 = 旧 shared_ptr 析构（refcount -1），可能触发还池；
 //   若 Upload 的 snapshot 副本还活着，refcount ≥ 1，帧继续存活。
 //
 // 索引约定（与常见「空一个槽」的 SPSC ring 不同）:
 //   head_  = 下一次写入位置
 //   count_ = 当前有效帧数（≤ capacity_）
 //   环满时：最老帧就在 head_（马上要被覆写的那个槽）
 //   环未满：最老帧在 (head_ - count_) mod capacity
 //
 // 线程模型（本白板版）:
 //   Recorder / Upload 可并发，靠 mutex_ 串行化 push / snapshot / size。
 //   热路径若不能接受锁：换成无锁 SPSC + 外部保证单写单快照，或 RCU。
 //
class VideoRingBuffer {
 public:
    explicit VideoRingBuffer(std::size_t capacity)
        : capacity_(capacity), slots_(capacity), count_(0), head_(0) {}

    // Recorder 线程：写入一帧。
    // 环未满 → 填下一个空槽，count_++。
    // 环已满 → 覆写最老槽（slots_[head_] 旧 shared_ptr 被替换）。
    void push(std::shared_ptr<Frame> frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        // move 进槽：本函数不再持有引用；槽成为 ring 侧的所有者之一。
        slots_[head_] = std::move(frame);
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) {
            ++count_;
        }
        // count_ == capacity_ 时不再增加：物理上一直满着，逻辑上滚动窗口。
    }

    // Upload 线程：拍一张「当前时间窗」的快照。
    // 返回值是 shared_ptr 的拷贝向量 → 每个槽 refcount +1。
    // 之后 Recorder 继续 push 覆写环内槽，也不会让 Upload 手里的帧悬空。
    std::vector<std::shared_ptr<Frame>> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Frame>> clip;
        clip.reserve(count_);

        // 计算「最老帧」的物理下标，再沿环走 count_ 步 → 时间序。
        //
        // 例 capacity=2，依次写入 A 再 B（环满）:
        //   push A → slots_[0]=A, head_=1, count_=1
        //   push B → slots_[1]=B, head_=0, count_=2
        //   满环时 head_ 即最老槽 → start=0 → clip=[A,B]
        //
        // 例只写了 A: head_=1, count_=1
        //   start = (1 + 2 - 1) % 2 = 0 → clip=[A]
        const std::size_t start =
            (count_ == capacity_) ? head_
                                  : (head_ + capacity_ - count_) % capacity_;

        for (std::size_t i = 0; i < count_; ++i) {
            const auto& frame = slots_[(start + i) % capacity_];
            if (frame) {
                // push_back 拷贝 shared_ptr：只动控制块，不动 Frame / 像素。
                clip.push_back(frame);
            }
        }
        return clip;  // NRVO / move；clip 内的 shared_ptr 继续活着
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

 private:
    const std::size_t capacity_;
    std::vector<std::shared_ptr<Frame>> slots_;
    std::size_t count_;   // 有效元素个数
    std::size_t head_;    // 下一写位置；满时也是最老元素位置
    mutable std::mutex mutex_;  // const snapshot()/size() 也要加锁 → mutable
};

// ---------------------------------------------------------------------------
 // 测试把「所有权交接」走完整条生命周期，对应上面五阶段。
 // ---------------------------------------------------------------------------
bool test_video_ring_buffer() {
    // 池 3、环 2：故意让池比环大，才能观察到
    // 「环覆写释放一帧，但 snapshot 仍握着它 → 还不能 recycle」。
    auto pool = std::make_shared<FreeList>(3);
    VideoRingBuffer ring(2);

    assert(pool->free_count() == 3);

    // --- 阶段 2/3：录两帧，环满 ---
    auto f1 = pool->acquire();
    f1->id = 101;
    ring.push(f1);
    f1.reset();  // Recorder 本地放手；环内 slots_ 仍持有 → 不 recycle

    auto f2 = pool->acquire();
    f2->id = 102;
    ring.push(f2);
    f2.reset();

    // 3 个池槽借出 2 个，剩 1；环持有 101、102
    assert(pool->free_count() == 1);
    assert(ring.size() == 2);

    // --- 阶段 4：Upload 快照 [101, 102] ---
    auto clip = ring.snapshot();
    assert(clip.size() == 2);
    assert(clip[0]->id == 101);
    assert(clip[1]->id == 102);
    // 此刻每个帧的所有者：ring 一份 + clip 一份（refcount ≥ 2）

    // --- 覆写最老槽 101：ring 放开 101，但 clip 仍握着 ---
    auto f3 = pool->acquire();
    f3->id = 103;
    ring.push(f3);  // 覆写 → ring 侧对 101 的 shared_ptr 析构（refcount -1）
    f3.reset();
    // 池已空（第三个也被借出）；101 还在 clip 里，所以 free_count 仍为 0
    assert(pool->free_count() == 0);

    // 新快照只看得到环里还活着的窗口：[102, 103]
    auto clip2 = ring.snapshot();
    assert(clip2.size() == 2);
    assert(clip2[0]->id == 102);
    assert(clip2[1]->id == 103);

    // --- 阶段 5：Upload 放掉第一段 clip → 101 的最后一个 shared_ptr 没了 ---
    clip.clear();  // refcount → 0 → 自定义 deleter → 还回 FreeList
    assert(pool->free_count() == 1);

    clip2.clear();
    // 102、103 仍在 ring 的 slots_ 里，不会回池
    assert(pool->free_count() == 1);

    return true;
}

int main() {
    assert(test_video_ring_buffer());
    std::cout << "video_ring_buffer: ok\n";
    return 0;
}
