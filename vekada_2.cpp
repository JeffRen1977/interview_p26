1. Core Pattern Problems (Hand-Coding Exercises)
In a 45–60 minute technical screen, you are almost guaranteed to write live C++11/14/17 code for one of these classic concurrent data structures.

A. Thread-Safe Bounded Queue / Ring Buffer
The Pitch: "Build a thread-safe frame buffer queue for an image pipeline where a camera producer thread pushes 4K YUV frames, and an AI inference thread pops them."

Key Requirements:

Fixed capacity (bounded) to prevent out-of-memory crashes on embedded devices.

Blocks or drops frames when full (push policy: block or overwrite oldest).

Uses std::mutex, std::condition_variable, and std::unique_lock.

Pro-Tip for Video Pipelines: Mention or implement an overwrite/drop strategy for real-time video (if the queue is full, drop the oldest frame so live preview latency doesn't build up).


class BoundingQueue{
private:
     int m_cap;
     vector<int> buffer;
     std::mutext mu;
     std::condition_variable m_full;
     std::condition_vairable m_empty;
     int head, tail;
  BoudingQueue(int cap):m_cap(cap){
     buffer.resize(m_cap);
     head=0;
     tail=0;
  }
  void produce(int item){
    std::unique_lock<std::mutex> lock(mu);
    if(buffer.size()==m_cap)
    {
      buffer[head]=item;
      head = (head++)%m_cap;
      return;
   }
    m_full.wait(lock,[this]{ return buffer.size()<m_cap;});      
    buffer[tail]=item;
    tail++;
    m_empty().notify_one();
 }
 int consume(){
    std::unique_lock<std::mutex> lock(mu);
    m_empty.wait(lock,[this]{ return !buffer.empty();});
    int item = buffer[head];
    head++;
    m_full().notify_one();
}
}

B. Producer-Consumer with Frame Dropping (Real-Time Video)
The Pitch: "Your ISP generates frames at 30 FPS, but your AI model can only run at 10 FPS. Implement a frame-skipping pipeline where the consumer always gets the latest available frame without blocking the producer."

Key Concepts: Single-element overwriting buffer with atomic flags or conditional variables signaling "new frame ready."

#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <vector>

// Dummy Frame structure (e.g., NV12 / YUV420 Image Buffer)
struct Frame {
    uint64_t frame_id{0};
    uint64_t timestamp_ms{0};
    std::vector<uint8_t> payload; // Pre-allocated image buffer

    Frame(size_t size = 1920 * 1080 * 1.5) : payload(size) {}
};

/**
 * Single-Element Overwriting Buffer for Real-Time Video Processing.
 * Guarantees zero producer blocking and latest-frame retrieval for the consumer.
 */
class RealtimeLatestFrameBuffer {
private:
    std::shared_ptr<Frame> latest_frame_{nullptr};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool new_frame_available_{false};
    bool is_shutdown_{false};

public:
    /**
     * Called by ISP Producer (30 FPS)
     * Overwrites any unconsumed frame and signals the AI Consumer.
     * Never blocks on the consumer.
     */
    void publish(std::shared_ptr<Frame> frame) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latest_frame_ = std::move(frame);
            new_frame_available_ = true;
        }
        cv_.notify_one(); // Wake up AI thread if it's waiting
    }

    /**
     * Called by AI Consumer (10 FPS)
     * Blocks until a new frame is ready, then returns the LATEST available frame.
     */
    std::shared_ptr<Frame> get_latest_blocking() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until there's a new frame or shutdown is requested
        cv_.wait(lock, [this]() { 
            return new_frame_available_ || is_shutdown_; 
        });

        if (is_shutdown_ && !new_frame_available_) {
            return nullptr;
        }

        // Consume the frame pointer and reset the readiness flag
        new_frame_available_ = false;
        return std::move(latest_frame_); // Leaves latest_frame_ as nullptr
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_shutdown_ = true;
        }
        cv_.notify_all();
    }
};

// ============================================================================
// Demonstration Simulation (ISP Producer vs AI Consumer)
// ============================================================================

int main() {
    RealtimeLatestFrameBuffer channel;

    // Simulate 3 pre-allocated frame buffers (Buffer Pool concept)
    std::vector<std::shared_ptr<Frame>> pool = {
        std::make_shared<Frame>(),
        std::make_shared<Frame>(),
        std::make_shared<Frame>()
    };

    // 1. Producer Thread: ISP Output @ 30 FPS (~33ms interval)
    std::thread producer([&]() {
        uint64_t frame_counter = 0;
        
        for (int i = 0; i < 30; ++i) { // Run for ~1 second
            auto start_time = std::chrono::steady_clock::now();
            
            // Acquire a buffer from pool (cycling for simulation)
            auto frame = pool[frame_counter % pool.size()];
            frame->frame_id = ++frame_counter;
            frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                start_time.time_since_epoch()).count();

            // Non-blocking push
            channel.publish(frame);
            std::cout << "[ISP Producer 30 FPS] Published Frame #" << frame->frame_id << std::endl;

            // Sleep ~33.3ms (30 FPS)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        channel.shutdown();
    });

    // 2. Consumer Thread: AI Inference @ 10 FPS (~100ms interval)
    std::thread consumer([&]() {
        while (true) {
            // Fetch the latest frame (skips any older unprocessed frames)
            auto frame = channel.get_latest_blocking();
            if (!frame) break; // Shutdown signal

            std::cout << "  ===> [AI Consumer 10 FPS] Processing Frame #" << frame->frame_id 
                      << " (Latest Available)" << std::endl;

            // Simulate heavy AI Inference work (~100ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "[AI Consumer] Exited cleanly." << std::endl;
    });

    producer.join();
    consumer.join();

    return 0;
}



C. Reader-Writer Lock for Camera Settings/Metadata
The Pitch: "Multiple video encoder threads continuously read camera configuration parameters (exposure, gain, Resolution), while one auto-exposure thread occasionally updates them. Implement a thread-safe config wrapper."

Key Concepts: std::shared_mutex (C++17) or std::shared_lock vs. std::unique_lock to avoid blocking reads when no writes are happening.

class cameraConfig{
   int exposure;
   int gain;
   int resolution[2];
}

class ReaderWriter{
private:
     cameraConfig camfig;
     std::share_mutex mu;
public:
    cameraConfig ReadConfig(){
     std::shared_lock<std::shared_mutext> sharedLock(mu);
    return camfig
     }
    void UpdateCameraConfig(){
    std::unique_lock<std::shared_mutex> uniqueLock(mu);
    //update the 
    camfig.exposure = 10;
}
    

