1. The Thread-Safe Bounded Queue (Producer-Consumer)
This is the #1 most common C++ concurrency problem in camera/imaging roles.
Scenario: A camera sensor thread continuously captures frame buffers (Producer) and pushes them into a buffer, while separate threads (Consumers) extract frames for video encoding, local motion analytics, or cloud upload.
Requirements:
Fixed max capacity (bounded queue) to prevent out-of-memory errors.
Thread-safe push() and pop().
Proper handling of condition variables to block push() when full, and block pop() when empty.
Clean thread shutdown / cancellation handling.

template <typname T>
class BlockQueue
{
   BlockQueue(int cap){
   capacity = cap;
   shutdown = false;
   }
void push(T frame_buffer){
    std::unique_lock<std::mutext> lock(mu);
    not_full.wait(lock,[this]{buffer.size()<capacity||shutdown;});
    buffer.push(frame_buffer);
    not.empty.notify_one();
}
T pop(){
    std::unique_lock<std::mutext> lock(mu);
    not_emtpy.wait(lock,[this]{buffer.size()>0||shut_down;});
    T buf=buffer.front();
    buffer.pop();
    not_full.notify_one();
    return buf
}
void shutdown(){
    std::unique_lock<std::mutext> lock(mu);
    shut_down=true;
}
private:
   queue<T> buffer;
   std::condition_variable not_full;
   std::condition_variaable not empty;
   int capapcity;
   std::mutex mu;
   bool shutdown;
}

// this is no lock solution, only using atomic to address this problem. 
template <typname T>
class Queues
{
   Queues(int cap){
      capacity = cap;
      shutdown = false;
      head_=0;
      tail_=0;
      buffer = vector<T>(capacity);   
   }
void push(T frame_buffer){
    int tail = tail_.load(std::memory_order_relaxed);
    int next_tail = (tail+1)%capacity;
    if (next_tail==head_.load(std:memory_order_acquired);
       return
    buffer[tail]=frame_buffer;
    tail_.store(next_tail,std::memory_order_released);
}
T pop(){
    int head = head_.load(std::memory_order_relaxed);
    if (head==tail_.load(std:memory_order_acquired);
       return
    T buf = buffer[head];
    head_.store((head+1)%capacity,std::memory_order_released);
    return buf
}
void shutdown(){
    std::unique_lock<std::mutext> lock(mu);
    shut_down=true;
}
private:
   vector<T> buffer;
   std::atmoic<int> head_;
   std::atmoic<int> tail_;
   int capapcity; 
   bool shutdown;
}
// Simulated Frame Buffer object
struct Frame {
    int frame_id;
    uint64_t timestamp_ms;
};

// Producer Worker Thread Function (e.g., Camera Sensor Capture)
void camera_producer_thread(Queues<Frame>& frame_queue, int total_frames) {
    for (int i = 1; i <= total_frames; ++i) {
        if (frame_queue.is_shutdown()) break;

        Frame new_frame{
            i, 
            static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            )
        };

        // Retry loop if buffer is full (Non-blocking polling)
        while (!frame_queue.push(new_frame)) {
            if (frame_queue.is_shutdown()) return;
            std::this_thread::yield(); // Yield CPU slice to avoid burning 100% core
        }

        std::cout << "[Camera Hardware] Captured Frame #" << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }
}

// Consumer Worker Thread Function (e.g., Image Processing / Analytics Pipeline)
void processing_consumer_thread(Queues<Frame>& frame_queue) {
    while (true) {
        std::optional<Frame> frame = frame_queue.pop();

        if (frame.has_value()) {
            std::cout << "  [ISP Pipeline] Processed Frame #" << frame->frame_id 
                      << " (Timestamp: " << frame->timestamp_ms << " ms)\n";
        } else {
            // Queue is empty. If system triggered shutdown, exit thread loop.
            if (frame_queue.is_shutdown()) {
                break;
            }
            std::this_thread::yield();
        }
    }
    std::cout << "  [ISP Pipeline] Thread shutting down cleanly.\n";
}

int main() {
    constexpr int QUEUE_CAPACITY = 8; // Holds max 7 items (SPSC circular array)
    constexpr int TOTAL_FRAMES_TO_STREAM = 15;

    Queues<Frame> frame_queue(QUEUE_CAPACITY);

    std::cout << "Starting Camera Streaming Pipeline...\n\n";

    // Launch Producer & Consumer Threads
    std::thread producer(camera_producer_thread, std::ref(frame_queue), TOTAL_FRAMES_TO_STREAM);
    std::thread consumer(processing_consumer_thread, std::ref(frame_queue));

    // Wait for the camera capture thread to finish streaming
    producer.join();

    // Signal pipeline to shut down once all items drain
    frame_queue.shutdown();

    // Wait for consumer thread to drain queue and terminate
    consumer.join();

    std::cout << "\nPipeline shutdown completed successfully.\n";
    return 0;
}

Readers-Writer Camera Configuration Lock
Scenario: Camera parameters (exposure, resolution, ISP digital gain, ROI bounds) are read tens of times per second by image processing pipelines, but updated infrequently by cloud commands or web clients.

Key Concept: Single-writer, multiple-reader access pattern using std::shared_mutex (C++17) or std::shared_timed_mutex (C++14).

Code Focus: Demonstrating std::shared_lock for read ops vs std::unique_lock for write ops.

#include <shared_mutex>
#include <mutex>

struct CameraConfig {
    int iso = 100;
    float exposure_time_ms = 16.6f;
    bool motion_detection_enabled = true;
};

class CameraSettingsManager {
private:
    CameraConfig config_;
    mutable std::shared_mutex rw_mutex_;

public:
    // Concurrent reads from multiple frame worker threads
    CameraConfig getConfig() const {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        return config_;
    }

    // Exclusive write when API updates parameters
    void updateExposure(int iso, float exposure) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        config_.iso = iso;
        config_.exposure_time_ms = exposure;
    }
};




