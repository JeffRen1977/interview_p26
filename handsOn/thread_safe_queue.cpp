class ThreadSafeQueue {
    private:
        std::queue<int> queue;
        std::mutex mutex;
        std::condition_variable cv_not_empty;
        std::condition_variable cv_not_full;
        const int max_size = 10;

    public:
        void push(int value) {
            std::unique_lock<std::mutex> lock(mutex);
            cv_not_full.wait(lock, [this] { return queue.size() < max_size; });
            queue.push(value);
            cv_not_empty.notify_one();
        }

        int pop() {
            std::unique_lock<std::mutex> lock(mutex);
            cv_not_empty.wait(lock, [this] { return !queue.empty(); });
            int value = queue.front();
            queue.pop();
            cv_not_full.notify_one();
            return value;
        }

        bool is_empty() {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.empty();
        }

        bool is_full() {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.size() == max_size;
        }
};