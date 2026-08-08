#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>

class ThreadPool {
public:
    // Initialize the pool with a set number of worker threads
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
    
    // Enqueue a task with perfect forwarding and return a future
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
        
    // Clean up and safely join all threads
    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop;
};

inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->cv.wait(lock, [this] { 
                        return this->stop || !this->tasks.empty(); 
                    });
                    
                    if (this->stop && this->tasks.empty()) {
                        return; // Exit thread execution loop
                    }
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task(); // Execute task outside of the lock context
            }
        });
    }
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    
    using return_type = typename std::invoke_result<F, Args...>::type;

    // Package the task into a shared pointer to make it copyable for std::function
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([task]() { (*task)(); });
    }
    cv.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    cv.notify_all(); // Wake up all threads to facilitate shutdown
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// Example Usage
int main() {
    ThreadPool pool(4); // Create a thread pool with 4 worker threads

    // Enqueue simple void tasks
    pool.enqueue([] { std::cout << "Task running asynchronously\n"; });

    // Enqueue a task that returns a value
    auto result = pool.enqueue([](int a, int b) { return a + b; }, 10, 20);

    // Retrieve the value asynchronously
    std::cout << "Result of math task: " << result.get() << std::endl;

    return 0;
}
