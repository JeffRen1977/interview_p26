// Problem: Producer-Consumer with Frame Dropping (real-time video)
//
 // Scenario (ISP → AI / XR 感知链路经典题):
 //   ISP / Camera 以 ~30 FPS 出帧；下游 AI 推理只能跑 ~10 FPS。
 //   消费端必须永远处理 *最新* 一帧；积压的旧帧直接丢掉。
 //   生产端绝不能被慢消费者堵住（publish 必须非阻塞）。
 //
 // 为何不能用普通有界队列？
 //   queue 会把旧帧都存下来 → 延迟越堆越高（tail latency 爆炸），
 //   或者生产者在满队列上 block → 丢的是「最新」而不是「最旧」。
 //   实时视觉要的是 freshness，不是完整序列。
 //
 // Design: 单槽 "latest frame" mailbox
 //   publish(frame): 覆写 slot_；旧 unique_ptr 析构 = 丢帧。永不等待。
 //   get_latest():   阻塞等到有新帧（或 shutdown），然后 *搬走* 所有权。
 //
 // Correctness 铁律:
 //   1) 用 unique_ptr 传递独占所有权。禁止 producer 写完还留着裸指针/
 //      shared 引用给 consumer —— Frame 缓冲常被环形复用，会 UAF。
 //   2) Dropping = 覆写 slot_。被挤掉的 unique_ptr 离开作用域即释放帧。
 //   3) has_new_ 区分「槽里有尚未取走的新帧」与「槽空 / 已取走」。
 //      不能只靠 slot_ != nullptr：shutdown 时槽可能非空但也不该再等。
 //   4) cv_.wait 用 predicate（while 语义），防止虚假唤醒。
 //
 // 和 video_ring_buffer 的对比:
 //   ring  = 保留一段历史（pre-event clip），多帧共享 shared_ptr。
 //   本题  = 只要最新一帧，历史无价值；unique_ptr 独占 + 覆写即丢。
 //
 // 面试追问 / Follow-ups:
 //   - Triple buffering: 3 个槽轮转，减少锁竞争、允许 DMA 与推理重叠。
 //   - 真像素: 槽里放 dma-buf fd / HIDL GraphicBuffer，不拷贝 payload。
 //   - 指标: drop_rate = 1 - consumed/published；告警阈值超时。
 //   - 多消费者: 本设计是 1P1C；多 AI 分支应对每个分支各做一个 mailbox，
 //     或 publish 时 clone/fd-dup，而不是共享同一 unique_ptr。
 //   - 与 condition_variable 相比，无锁版可用 atomic<shared_ptr> /
 //     hazard pointer 交换槽指针（C++20 atomic<shared_ptr>）。

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

struct Frame {
    std::uint64_t frame_id{0};
    std::uint64_t timestamp_ms{0};
    std::vector<std::uint8_t> payload;  // 白板用小 vector；产线是 DMA 缓冲

    explicit Frame(std::size_t bytes = 64) : payload(bytes) {}
};

// ---------------------------------------------------------------------------
// LatestFrameBuffer: 单槽最新帧邮箱（mailbox）
// ---------------------------------------------------------------------------
//
 // 线程模型: 典型 1 Producer + 1 Consumer。
 // mutex_ 保护 slot_ / has_new_ / 计数；cv_ 只在「从无新帧 → 有新帧」
 // 或 shutdown 时唤醒消费者。
 //
 // 生命周期示意:
 //   publish(F3) 时若 slot_ 仍持有 F2（消费者还没取）:
 //       unique_ptr 赋值 → F2 析构 → 帧 2 被 drop
 //       slot_ = F3, has_new_ = true, notify
 //   get_latest():
 //       wait until has_new_
 //       has_new_ = false
 //       return move(slot_)  → 槽空，所有权交给调用方
 //
class LatestFrameBuffer {
 public:
    // 生产者热路径：非阻塞。
    // 无论消费者多慢，这里只做一次加锁赋值 + notify，然后立刻返回。
    void publish(std::unique_ptr<Frame> frame) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 关键关键 丢帧发生点 关键────────────────────────────────
            // 若上一帧还没被 get_latest() 取走，这里的赋值会销毁旧
            // unique_ptr，帧内存/DMA 句柄随之释放。这是刻意的 drop，
            // 不是泄漏。published_ 仍 +1，便于事后算 drop rate。
            // ────────────────────────────────────────────────────
            slot_ = std::move(frame);
            has_new_ = true;
            ++published_;
        }
        // notify 放在锁外：被唤醒的消费者立刻去抢锁，减少无谓切换。
        // （持锁 notify 也正确，只是稍微多占一会儿临界区。）
        cv_.notify_one();
    }

    // 消费者：阻塞直到有新帧，或 shutdown。
    // 返回值:
    //   non-null → 独占拥有该 Frame，用完即释放（或还回对象池）。
    //   nullptr  → 只在 shutdown 且当前没有未取新帧时出现，表示退出。
    std::unique_ptr<Frame> get_latest() {
        std::unique_lock<std::mutex> lock(mutex_);

        // wait(lock, pred) ≡ while (!pred()) wait(lock);
        // 必须用谓词：虚假唤醒、以及「notify 时已经有人取走」都要再检查。
        cv_.wait(lock, [this] { return has_new_ || shutdown_; });

        // shutdown 且没有尚未消费的新帧 → 让上层跳出循环。
        // 注意：shutdown 瞬间若 has_new_==true，仍应先把最后一帧拿走
        // （本实现优先交付；若想「关机立即弃帧」可改成先看 shutdown_）。
        if (!has_new_) {
            return nullptr;
        }

        has_new_ = false;
        ++consumed_;
        // move 后 slot_ 变为空；下一轮 publish 写入的是全新所有权。
        // 消费者持有期间，producer 再 publish 只会往空槽塞新帧，
        // 不会和消费者手里的 Frame 别名（独占保证）。
        return std::move(slot_);
    }

    // 结束消费循环。必须 notify_all：消费者可能正堵在 wait 上。
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    std::uint64_t published() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return published_;
    }

    std::uint64_t consumed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return consumed_;
    }

 private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::unique_ptr<Frame> slot_;  // 至多一帧；空 = 已被取走或尚未写入
    bool has_new_{false};          // 槽内是否有「尚未 get 过」的帧
    bool shutdown_{false};

    // 监控用。drop ≈ published_ - consumed_（单槽下近似，忽略 inflight）。
    std::uint64_t published_{0};
    std::uint64_t consumed_{0};
};

// ---------------------------------------------------------------------------
// 单线程语义测试：连续 publish 三次，只应拿到最后一帧。
// ---------------------------------------------------------------------------
bool test_overwrite_semantics() {
    LatestFrameBuffer buf;

    auto make = [](std::uint64_t id) {
        auto f = std::make_unique<Frame>();
        f->frame_id = id;
        return f;
    };

    // 无消费者 intervening → 1、2 都被 3 覆写丢掉
    buf.publish(make(1));
    buf.publish(make(2));
    buf.publish(make(3));

    auto got = buf.get_latest();
    assert(got != nullptr);
    assert(got->frame_id == 3);  // freshness：只要最新
    assert(buf.published() == 3);
    assert(buf.consumed() == 1);

    buf.publish(make(4));
    got = buf.get_latest();
    assert(got && got->frame_id == 4);

    // shutdown 后，若没有未取新帧，get_latest 返回 nullptr 以结束循环
    buf.shutdown();
    assert(buf.get_latest() == nullptr);
    return true;
}

int main() {
    assert(test_overwrite_semantics());

    // -----------------------------------------------------------------------
    // 并发演示：生产者 ~每 5ms 一帧，消费者处理故意睡 20ms。
    // 速率差 → 必然 drop；断言 consumed < published，且帧号单调递增
    // （不会出现「先拿到 10 再拿到 7」这种过期帧）。
    // -----------------------------------------------------------------------
    LatestFrameBuffer channel;
    constexpr int kProduce = 30;

    std::thread producer([&] {
        for (int i = 1; i <= kProduce; ++i) {
            auto frame = std::make_unique<Frame>();
            frame->frame_id = static_cast<std::uint64_t>(i);
            frame->timestamp_ms = static_cast<std::uint64_t>(i * 33);  // ~30FPS
            // move 进 mailbox：此后 producer 不再碰这块 Frame
            channel.publish(std::move(frame));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        channel.shutdown();  // 让消费者从 wait 醒来并退出
    });

    std::thread consumer([&] {
        std::uint64_t last = 0;
        while (true) {
            auto frame = channel.get_latest();
            if (!frame) {
                break;  // shutdown 且无剩余新帧
            }
            // 单槽 + 独占所有权 ⇒ 每次拿到的 id 必须严格变新
            assert(frame->frame_id > last);
            last = frame->frame_id;
            // 模拟慢推理：比生产慢 4× → 大量中间帧在 publish 时被 drop
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        assert(last >= 1);
    });

    producer.join();
    consumer.join();

    assert(channel.published() == static_cast<std::uint64_t>(kProduce));
    assert(channel.consumed() <= channel.published());
    // 以本测试的速率差，几乎必然发生丢帧；用来锁住「drop 真的发生了」
    assert(channel.consumed() < channel.published());

    std::cout << "producer_consumer_frame_dropping: ok (published="
              << channel.published() << " consumed=" << channel.consumed()
              << ")\n";
    return 0;
}
