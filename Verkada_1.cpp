1. The Thread-Safe Bounded Queue (Producer-Consumer)
This is the #1 most common C++ concurrency problem in camera/imaging roles.
Scenario: A camera sensor thread continuously captures frame buffers (Producer) and pushes them into a buffer, while separate threads (Consumers) extract frames for video encoding, local motion analytics, or cloud upload.
Requirements:
Fixed max capacity (bounded queue) to prevent out-of-memory errors.
Thread-safe push() and pop().
Proper handling of condition variables to block push() when full, and block pop() when empty.
Clean thread shutdown / cancellation handling.

class workers
{
   workers(int cap){
   capacity = cap;
   }
void push(FrameBuffer frame_buffer){
    std::unique_lock<std::mutext> lock(mu);
    not_full.wait(lock,[this]{buffer.size()<capacity;});
    buffer.push(frame_buffer);
    not.empty.notify_one();
}
Frame pop(){
    std::unique_lock<std::mutext> lock(mu);
    not_emtpy.wait(lock,[this]{buffer.size()>0;});
    Frame buf buffer.front();
    buffer.pop();
    not_full.notify_one();
}
private:
   queue<Frame> buffer;
   std::condition_wait not_full;
   std::condition_wait not empty;
   int capapcity;
   std::mutex mu;
}
