// Problem: Local SD Card Writer + fan-out camera pipeline (I/O bound)
//
 // Scenario (车载 / 安防多路消费经典题):
 //   ISP 每拍一帧是 ~8MB。同一帧要同时送给三条下游链路:
 //     1) H.264 编码器  — 可靠、可阻塞（不能随便丢 GOP 参考帧）
 //     2) AI 检测器     — 要新鲜度；忙不过来就丢最旧
 //     3) SD 卡落盘     — 可靠本地 I/O，写卡慢，允许阻塞
 //
 // 核心约束:
 //   - 禁止对 8MB 像素做 memcpy / 禁止每条链路各自 new 一份缓冲
 //     → 三路队列里放的都是同一个 shared_ptr<Frame>（只动引用计数）
 //   - 每条消费者有自己的队列 + 自己的反压策略（可靠 vs 可丢）
 //   - 有界阻塞队列的 CV 唤醒要配对：push 唤醒 pop，pop 唤醒 push
 //
 // 和另外两题的关系:
 //   producer_consumer_frame_dropping = 单槽 mailbox，只要最新一帧
 //   video_ring_buffer                 = 环形历史窗，snapshot 共享所有权
 //   本题                             = 一产多消 fan-out，策略按 sink 分化
 //
 // 面试官常追问:
 //   - 为何 AI 用 drop-oldest 而不是 drop-newest？实时检测要最新画面。
 //   - dispatch 里先 blocking 再 drop：H264 满会拖住 AI/SD 入队吗？
 //     会。生产可改成先尝试非阻塞 fan-out / 每路独立 dispatcher 线程。
 //   - shared_ptr 控制块在多核上的原子 refcount 也有开销；极致数据面
 //     可用 intrusive refcount 或「帧池 + 手动 borrow/return」。
 //   - SD 卡写入失败 / 卡满：可靠队列会反压到 ISP，需要旁路策略
 //     （降分辨率、只写事件片段、切到 ring buffer）。
 //   - shutdown 必须同时 notify not_empty_ 和 not_full_，否则一边永久卡死。

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

struct Frame {
    std::uint64_t frame_id{0};
    // 白板用小 vector；真实系统是 8MB NV12 / dma-buf fd，拷贝代价极高。
    std::vector<std::uint8_t> pixel_data;

    explicit Frame(std::uint64_t id, std::size_t bytes = 64)
        : frame_id(id), pixel_data(bytes) {}
};

// ---------------------------------------------------------------------------
// BoundedQueue<T>: 有界队列，两种入队策略共用同一套存储
// ---------------------------------------------------------------------------
//
 // 两个 CV 的分工（经典有界阻塞队列）:
 //   not_empty_ — 队列从空→非空时唤醒消费者（pop 侧）
 //   not_full_  — 队列从满→有空位时唤醒生产者（push_blocking 侧）
 //
 // 易错点: pop 之后如果忘了 not_full_.notify_one()，
 // push_blocking 会在满队列上永久 wait（见 test_queue_cv_wakeup）。
 //
template <typename T>
class BoundedQueue {
 public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        assert(capacity_ > 0);
    }

    // 可靠入队：队列满则阻塞，直到有空位或 shutdown。
    // 用于 H264 / SD —— 宁可让上游慢一点，也不丢必须保留的帧。
    void push_blocking(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // 谓词必须包含 shutdown_：否则 stop() 后生产者可能永远等不满条件。
        not_full_.wait(lock, [this] {
            return queue_.size() < capacity_ || shutdown_;
        });
        if (shutdown_) {
            // 关停后直接丢弃本次入队（item 在返回时析构）。
            // 若要求「关停前排空」，应在 shutdown 前 join 生产侧，或
            // 这里改成仍允许把已有数据塞完再拒绝新帧。
            return;
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();  // 可能有消费者堵在空队列上
    }

    // 实时入队：永不阻塞。满则丢掉队头（最旧），再 push 新帧。
    // 用于 AI —— 检测永远想看最新画面；积压的旧帧没有推理价值。
    void push_drop_oldest(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) {
            return;
        }
        if (queue_.size() >= capacity_) {
            queue_.pop();  // 丢最旧；该 shared_ptr refcount -1
            ++dropped_;
            // 注意: 这里队列仍是满→即将再满，没有「空出给 blocking waiter」
            // 的语义需求；且本路径不走 not_full_ wait，所以不必 notify_full。
            // 但若同队列混用两种 push，严格说 pop 旧元素后应 notify not_full_。
            // 本题 AI 队列只用 drop 路径，保持简单。
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();
    }

    // 出队。shutdown 且队列已空 → 返回 false，让 drain 循环退出。
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        if (queue_.empty()) {
            return false;  // 只可能是 shutdown
        }
        item = std::move(queue_.front());
        queue_.pop();
        // ★ 关键点: 腾出一个槽后必须唤醒可能堵在 push_blocking 上的生产者。
        // 只 notify not_empty_ 不够——那是给其他消费者的。
        not_full_.notify_one();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        // 两边都要喊醒：空等的消费者 + 满等的生产者。
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t dropped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

 private:
    const std::size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    bool shutdown_{false};
    std::size_t dropped_{0};  // 仅 push_drop_oldest 路径累加
};

// ---------------------------------------------------------------------------
// CameraPipeline: 一帧 fan-out 到三条差异化消费链路
// ---------------------------------------------------------------------------
//
 // 启动时拉起 3 个 drain 线程，各自绑定一条队列。
 // dispatch() 在 ISP / 采集线程上调用：把同一个 shared_ptr 按策略入三队。
 //
 // 引用计数直觉（单帧）:
 //   make_shared          → use_count = 1
 //   push 进 h264_q_      → 队列里拷贝一份，use_count = 2
 //   push 进 ai_q_        → use_count = 3
 //   push 进 sd_q_        → use_count = 4
 //   dispatch 返回后局部 shared_ptr 析构 → use_count = 3（三队各一）
 //   某消费者 pop 并处理完 reset → 该路 -1；三路都放完 → Frame 析构
 //
 // 全程没有 pixel_data 的深拷贝，只有控制块上的原子加减。
 //
class CameraPipeline {
 public:
    CameraPipeline() {
        h264_ = std::thread([this] { drain(h264_q_, "H264"); });
        ai_ = std::thread([this] { drain(ai_q_, "AI"); });
        sd_ = std::thread([this] { drain(sd_q_, "SD"); });
    }

    // 析构兜底 stop，避免线程还在跑时成员队列已销毁（UB）。
    ~CameraPipeline() { stop(); }

    // Fan-out 入口。传入 shared_ptr（可 move），内部按值入队产生拷贝。
    void dispatch(std::shared_ptr<Frame> frame) {
        // 顺序有意为之，也是面试讨论点:
        //   先 H264（blocking）→ 再 AI（drop）→ 再 SD（blocking）
        // 若 H264 队列满，这里会卡住，AI/SD 暂时也拿不到这一帧。
        // 产线常见改法:
        //   - 每路一个无锁/短队列 SPSC，采集线程只做非阻塞 try_push
        //   - 或「dispatcher 线程」从单队列再 fan-out，隔离 ISP 热路径
        h264_q_.push_blocking(frame);
        ai_q_.push_drop_oldest(frame);
        sd_q_.push_blocking(std::move(frame));  // 最后一次可 move，少一次原子加
    }

    void stop() {
        // 先关队列（唤醒所有 wait），再 join，避免 join 死等。
        h264_q_.shutdown();
        ai_q_.shutdown();
        sd_q_.shutdown();
        if (h264_.joinable()) {
            h264_.join();
        }
        if (ai_.joinable()) {
            ai_.join();
        }
        if (sd_.joinable()) {
            sd_.join();
        }
    }

    std::size_t ai_dropped() const { return ai_q_.dropped(); }

 private:
    // 通用消费循环：pop 失败（shutdown+空）即退出。
    // name 参数留给日志/打点；本白板版刻意不用，避免刷屏。
    void drain(BoundedQueue<std::shared_ptr<Frame>>& q, const char* name) {
        std::shared_ptr<Frame> frame;
        while (q.pop(frame)) {
            (void)name;
            // 模拟编码 / 推理 / fwrite。
            // frame 在本轮末尾离开作用域 → 该路 shared_ptr 释放；
            // 若另两路还握着同一 Frame，像素缓冲继续存活。
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ++processed_;
        }
    }

    // 容量选择本身就是策略:
    //   H264/SD = 4  —— 吸收短时抖动，满了就反压采集
    //   AI      = 1  —— 几乎不排队；稍慢就触发 drop-oldest（演示丢帧）
    BoundedQueue<std::shared_ptr<Frame>> h264_q_{4};
    BoundedQueue<std::shared_ptr<Frame>> ai_q_{1};
    BoundedQueue<std::shared_ptr<Frame>> sd_q_{4};

    std::thread h264_;
    std::thread ai_;
    std::thread sd_;
    std::atomic<int> processed_{0};  // 三路合计处理次数（监控用）
};

// ---------------------------------------------------------------------------
 // CV 配对回归: 容量=1 时，第二次 push_blocking 必须靠 pop 的 notify 才能继续。
 // 若 pop 漏掉 not_full_.notify_one()，本测试会挂死。
 // ---------------------------------------------------------------------------
bool test_queue_cv_wakeup() {
    BoundedQueue<int> q(1);
    std::thread producer([&] {
        q.push_blocking(1);
        q.push_blocking(2);  // 队列已满 → 阻塞，等消费者 pop 腾位
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int v = 0;
    assert(q.pop(v) && v == 1);  // 这一下必须唤醒生产者
    assert(q.pop(v) && v == 2);
    producer.join();
    q.shutdown();
    return true;
}

int main() {
    assert(test_queue_cv_wakeup());

    CameraPipeline pipeline;
    for (std::uint64_t i = 1; i <= 8; ++i) {
        auto frame = std::make_shared<Frame>(i);
        // dispatch 前 use_count == 1；三路入队后各持一份拷贝。
        // move 进 dispatch 避免调用方再留一份无用引用。
        pipeline.dispatch(std::move(frame));
        // 采集略快于单路 drain，AI 容量=1 → 容易观察到 ai_dropped > 0
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    pipeline.stop();

    std::cout << "local_sd_card_writer: ok (ai_dropped=" << pipeline.ai_dropped()
              << ")\n";
    return 0;
}
