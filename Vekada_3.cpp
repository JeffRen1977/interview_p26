A. Design an In-Memory Video Ring Buffer (Rolling Clip Capture)
Scenario: Verkada cameras save 30 seconds of video before a motion event occurs (pre-buffer continuous recording). How do you design the memory allocation and concurrency model?


#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>

// 视频帧数据块（假设每块包含 256KB 画面）
struct Frame {
    int id;
};

// ============================================================================
// 1. FreeList (内存池)
// ============================================================================
class FreeList : public std::enable_shared_from_this<FreeList> {
private:
    std::queue<std::unique_ptr<Frame>> pool_;
    std::mutex mutex_;

public:
    FreeList(int capacity) {
        // 【阶段 1：闲置期】预分配固定内存，静静躺在 FreeList 里
        for (int i = 0; i < capacity; ++i) {
            pool_.push(std::make_unique<Frame>());
        }
    }

    std::shared_ptr<Frame> acquire() {
        std::unique_ptr<Frame> raw_frame;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pool_.empty()) return nullptr;
            raw_frame = std::move(pool_.front());
            pool_.pop();
        }

        // 【关键点】借出内存时，绑定 Custom Deleter！
        // 记住当前 FreeList 的弱引用，防止循环引用
        std::weak_ptr<FreeList> weak_self = shared_from_this();

        return std::shared_ptr<Frame>(
            raw_frame.release(),
            [weak_self](Frame* ptr) {
                // 【阶段 5：回收期】当 shared_ptr 引用计数归零时，自动触发这里的 Lambda 函数
                if (auto self = weak_self.lock()) {
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    self->pool_.push(std::unique_ptr<Frame>(ptr)); // 重新放回池子
                    std::cout << "  [FreeList] Frame #" << ptr->id << " 已被自动回收归还到 FreeList!\n";
                } else {
                    delete ptr; // 如果池子销毁了，才真正 delete
                }
            }
        );
    }

    size_t free_count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }
};

// ============================================================================
// 2. RingBuffer (环形缓冲区)
// ============================================================================
class RingBuffer {
private:
    std::vector<std::shared_ptr<Frame>> buffer_;
    size_t head_ = 0;
    size_t capacity_;
    std::mutex mutex_;

public:
    RingBuffer(size_t capacity) : capacity_(capacity), buffer_(capacity) {}

    // 【阶段 3：缓存期】Recorder 线程把 shared_ptr 传给 RingBuffer
    void push(std::shared_ptr<Frame> frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 赋值覆盖！若原 slot 有旧 frame，原 frame 的 shared_ptr 引用计数会 -1
        buffer_[head_] = std::move(frame); 
        head_ = (head_ + 1) % capacity_;
    }

    // 【阶段 4：上传期】触发运动检测事件，导出快照
    std::vector<std::shared_ptr<Frame>> snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Frame>> clip;
        for (const auto& frame : buffer_) {
            if (frame) {
                clip.push_back(frame); // 拷贝 shared_ptr，导致该 Frame 的引用计数 +1 (变为 2)
            }
        }
        return clip; // 极速返回指针数组（零数据拷贝）
    }
};

// ============================================================================
// 3. 运行测试流程（演示 5 个阶段）
// ============================================================================
int main() {
    // 假设内存池容量为 3，RingBuffer 大小为 2
    auto pool = std::make_shared<FreeList>(3);
    RingBuffer ring(2);

    std::cout << "初始状态 FreeList 剩余: " << pool->free_count() << "\n\n";

    // ------------------------------------------------------------------------
    // 生产 Frame 1
    // ------------------------------------------------------------------------
    auto f1 = pool->acquire(); // 【阶段 2：生产期】从 FreeList 借出 f1 (Ref = 1)
    f1->id = 101;
    ring.push(f1);             // 【阶段 3：缓存期】放入 RingBuffer Slot 0 (Ref = 1)
    f1.reset();                // Recorder 局部变量销毁，仅 RingBuffer 持有 (Ref = 1)

    // 生产 Frame 2
    auto f2 = pool->acquire(); 
    f2->id = 102;
    ring.push(f2);             // 放入 RingBuffer Slot 1 (Ref = 1)
    f2.reset();

    std::cout << "写入 2 个 Frame 后，FreeList 剩余: " << pool->free_count() << "\n\n";

    // ------------------------------------------------------------------------
    // 触发事件：云端上传线程抓取快照
    // ------------------------------------------------------------------------
    std::cout << ">>> 触发 Motion Event! 抓取剪辑... <<<\n";
    // 【阶段 4：上传期】拷贝指针数组，此时 Frame 101 和 102 的引用计数全变为 2
    std::vector<std::shared_ptr<Frame>> upload_clip = ring.snapshot(); 
    std::cout << "Upload 线程拿到了 " << upload_clip.size() << " 个 Frame 指针\n\n";

    // ------------------------------------------------------------------------
    // 此时 Recorder 还在继续跑，写入 Frame 3 覆盖 Slot 0 (原本存着 Frame 101)
    // ------------------------------------------------------------------------
    std::cout << ">>> Recorder 线程写入 Frame 3，覆盖 Slot 0 (原本的 101)... <<<\n";
    auto f3 = pool->acquire();
    f3->id = 103;
    ring.push(f3); // RingBuffer 不再持有 101 (101 的 Ref 从 2 降为 1)
    f3.reset();

    std::cout << "【验证点】虽然 101 被 RingBuffer 覆盖了，但上传线程还持有着它，所以 101 没有被回收！\n";
    std::cout << "当前 FreeList 剩余: " << pool->free_count() << "\n\n";

    // ------------------------------------------------------------------------
    // 上传线程处理完毕，释放剪辑
    // ------------------------------------------------------------------------
    std::cout << ">>> Upload 线程处理完毕，释放剪辑内存... <<<\n";
    upload_clip.clear(); // 【阶段 5：回收期】101 的引用计数从 1 降为 0！触发 Custom Deleter!

    std::cout << "\n最终 FreeList 剩余: " << pool->free_count() << " (101 已安全收回供后续循环使用)\n";

    return 0;
}
