1. The Thread-Safe Bounded Queue (Producer-Consumer)
This is the #1 most common C++ concurrency problem in camera/imaging roles.
Scenario: A camera sensor thread continuously captures frame buffers (Producer) and pushes them into a buffer, while separate threads (Consumers) extract frames for video encoding, local motion analytics, or cloud upload.
Requirements:
Fixed max capacity (bounded queue) to prevent out-of-memory errors.
Thread-safe push() and pop().
Proper handling of condition variables to block push() when full, and block pop() when empty.
Clean thread shutdown / cancellation handling.

template <typname T>
class workers
{
   workers(int cap){
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


