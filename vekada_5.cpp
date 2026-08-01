Local SD Card Writer (I/O Bound)
What they look for:
How do you avoid duplicating 8MB raw frame buffers in memory? (Reference counting / Thread-safe ref-counted wrappers).
Fan-out concurrency patterns (one producer, multiple parallel consumers with synchronization points). please provide C++ simple implementation


#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// 1. Single 8MB Raw Frame Buffer
struct Frame {
    uint64_t frame_id;
    std::vector<uint8_t> pixel_data;

    // Pre-allocate 8MB payload
    explicit Frame(uint64_t id, size_t size = 8 * 1024 * 1024) 
        : frame_id(id), pixel_data(size) {}

    ~Frame() {
        std::cout << "  [Memory Recycler] Frame #" << frame_id 
                  << " Ref-Count reached 0 -> Buffer freed/recycled!\n";
    }
};

// 2. Thread-Safe Queue for Parallel Pipeline Nodes
template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_capacity_;
    bool shutdown_{false};

public:
    explicit ThreadSafeQueue(size_t capacity) : max_capacity_(capacity) {}

    // Reliable Push (Blocks if full) -> Used for SD Card & H.264 Encoder
    void push_blocking(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return queue_.size() < max_capacity_ || shutdown_; });
        if (shutdown_) return;
        queue_.push(std::move(item));
        cv_.notify_one();
    }

    // Real-Time Push (Drops oldest if full) -> Used for AI Inference
    void push_or_drop_oldest(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        if (queue_.size() >= max_capacity_) {
            queue_.pop(); // Drop oldest frame to maintain low latency
        }
        queue_.push(std::move(item));
        cv_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });
        if (shutdown_ && queue_.empty()) return false;

        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }
};

// 3. Fan-Out Pipeline Dispatcher
class CameraPipelineDispatcher {
private:
    // Decoupled Queues for Parallel Workers
    ThreadSafeQueue<std::shared_ptr<Frame>> h264_queue_{5};
    ThreadSafeQueue<std::shared_ptr<Frame>> ai_queue_{2};   // Small depth: drop frames if busy
    ThreadSafeQueue<std::shared_ptr<Frame>> sd_queue_{5};   // Reliable write queue

    std::thread h264_thread_;
    std::thread ai_thread_;
    std::thread sd_thread_;

public:
    CameraPipelineDispatcher() {
        start_workers();
    }

    ~CameraPipelineDispatcher() {
        stop();
    }

    // FAN-OUT: One producer pushes the SAME pointer to 3 independent nodes
    void dispatch_frame(std::shared_ptr<Frame> frame) {
        std::cout << "\n[ISP Producer] Fan-Out Frame #" << frame->frame_id 
                  << " (Address: " << frame.get() 
                  << " | Ref-Count: " << frame.use_count() << ")\n";

        // 1. Push to H.264 Hardware Encoder Node (Blocking)
        h264_queue_.push_blocking(frame);

        // 2. Push to AI Detection Node (Drop-Oldest if AI is busy)
        ai_queue_.push_or_drop_oldest(frame);

        // 3. Push to SD Card Writer Node (Blocking - I/O bound)
        sd_queue_.push_blocking(frame);
    }

    void stop() {
        h264_queue_.shutdown();
        ai_queue_.shutdown();
        sd_queue_.shutdown();

        if (h264_thread_.joinable()) h264_thread_.join();
        if (ai_thread_.joinable()) ai_thread_.join();
        if (sd_thread_.joinable()) sd_thread_.join();
    }

private:
    void start_workers() {
        // Consumer Node 1: H.264 Encoder (DSP)
        h264_thread_ = std::thread([this]() {
            std::shared_ptr<Frame> frame;
            while (h264_queue_.pop(frame)) {
                std::cout << "  ├─ [H.264 DSP] Encoding Frame #" << frame->frame_id 
                          << " (Ref-Count = " << frame.use_count() << ")\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        });

        // Consumer Node 2: AI Engine (NPU)
        ai_thread_ = std::thread([this]() {
            std::shared_ptr<Frame> frame;
            while (ai_queue_.pop(frame)) {
                std::cout << "  ├─ [NPU AI] Face Detection Frame #" << frame->frame_id 
                          << " (Ref-Count = " << frame.use_count() << ")\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(70)); // Slow AI task
            }
        });

        // Consumer Node 3: Local SD Card Writer (I/O Bound)
        sd_thread_ = std::thread([this]() {
            std::shared_ptr<Frame> frame;
            while (sd_queue_.pop(frame)) {
                std::cout << "  └─ [SD Card Writer] Writing Frame #" << frame->frame_id 
                          << " to Disk (Ref-Count = " << frame.use_count() << ")\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(30)); // I/O Delay
            }
        });
    }
};

// 4. Test Simulation Driver
int main() {
    CameraPipelineDispatcher pipeline;

    // Simulate ISP Producing 4 Frames @ 30 FPS
    for (uint64_t i = 1; i <= 4; ++i) {
        // Allocate 8MB ONCE
        auto frame = std::make_shared<Frame>(i);
        
        pipeline.dispatch_frame(frame);

        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    pipeline.stop();

    return 0;
}
