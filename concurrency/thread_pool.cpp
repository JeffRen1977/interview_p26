// Problem: Fixed-size thread pool with futures and graceful shutdown.
//
// This is the single most-asked C++ concurrency interview question. If you can
// write this in 15 minutes with correct shutdown semantics, you clear the bar.
//
// Scenario (Verkada-flavored):
//   Motion events arrive from N cameras. Each event needs CPU work (decode a
//   thumbnail, run a classifier). You do NOT want one thread per event -- that
//   is unbounded thread creation. Bound the workers, queue the work.
//
// Whiteboard talking points:
// - State: task queue + mutex + one CV + a `stop_` flag.
// - Workers loop: wait until (task available || stop). Never wait on a bare if.
// - submit() returns std::future so the caller can get a result or an exception.
//   packaged_task is the glue: it captures the callable and fulfills the promise.
// - packaged_task<void()> is move-only, and std::function requires the target to
//   be copy-constructible -> wrap it in shared_ptr (the classic trick), or use a
//   move-only function wrapper (shown at the bottom).
// - Shutdown must: set stop_, notify_all, then join. Draining vs. dropping
//   pending tasks is a policy decision -- state which one you chose.
// - Destructor must join, or std::thread's destructor calls std::terminate.
// - Deadlock trap: never hold the lock while invoking the task.

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// ThreadPool
// ---------------------------------------------------------------------------
class ThreadPool {
 public:
    explicit ThreadPool(std::size_t num_threads =
                            std::thread::hardware_concurrency()) {
        if (num_threads == 0) {
            num_threads = 1;  // hardware_concurrency() may return 0.
        }
        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    // Non-copyable, non-movable: threads capture `this`.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() { shutdown(); }

    // Submit any callable; get a future for its result.
    //
    // Return type deduction: C++17 uses std::invoke_result_t. (In C++11/14 you
    // would write typename std::result_of<F(Args...)>::type.)
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using Ret = std::invoke_result_t<F, Args...>;

        // Bind arguments now so the worker just calls a nullary task.
        auto task = std::make_shared<std::packaged_task<Ret()>>(
            [func = std::forward<F>(f),
             tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Ret {
                return std::apply(std::move(func), std::move(tup));
            });

        std::future<Ret> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                // Submitting after shutdown is a programming error. Throwing is
                // better than silently dropping: the caller's future would
                // otherwise never become ready and they would hang in get().
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            // shared_ptr capture makes this lambda copy-constructible, which
            // std::function requires.
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    // Set stop, wake everyone, join. Idempotent.
    // Policy here: DRAIN -- already-queued tasks still run. To discard instead,
    // clear tasks_ while holding the lock before notify_all().
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                return;
            }
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& w : workers_) {
            if (w.joinable()) {
                w.join();
            }
        }
    }

    std::size_t size() const { return workers_.size(); }

    std::size_t pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    std::uint64_t completed() const {
        return completed_.load(std::memory_order_relaxed);
    }

 private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

                // Drain policy: only exit once the queue is actually empty.
                // (Discard policy would be: if (stop_) return;)
                if (stop_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            // Lock is released here on purpose. Running user code under the
            // pool mutex would serialize the whole pool -- and if the task
            // called submit(), it would self-deadlock on a non-recursive mutex.
            task();
            completed_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::atomic<std::uint64_t> completed_{0};
};

// ---------------------------------------------------------------------------
// Follow-up they may ask: avoid the shared_ptr allocation.
// A minimal move-only callable wrapper -- what std::move_only_function is in
// C++23. Mention this if asked "can you do it without the shared_ptr?".
// ---------------------------------------------------------------------------
class MoveOnlyTask {
 public:
    MoveOnlyTask() = default;

    template <typename F>
    MoveOnlyTask(F&& f) : impl_(new Model<F>(std::forward<F>(f))) {}

    void operator()() { impl_->call(); }
    explicit operator bool() const { return impl_ != nullptr; }

 private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() = 0;
    };
    template <typename F>
    struct Model : Concept {
        explicit Model(F&& f) : fn(std::move(f)) {}
        void call() override { fn(); }
        F fn;
    };
    std::unique_ptr<Concept> impl_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
void test_returns_values() {
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([i] { return i * i; }));
    }
    for (int i = 0; i < 100; ++i) {
        assert(futures[i].get() == i * i);
    }
}

void test_arguments_and_void() {
    ThreadPool pool(2);
    auto sum = pool.submit([](int a, int b) { return a + b; }, 20, 22);
    assert(sum.get() == 42);

    std::atomic<int> counter{0};
    std::vector<std::future<void>> done;
    for (int i = 0; i < 50; ++i) {
        done.push_back(pool.submit([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (auto& f : done) {
        f.get();
    }
    assert(counter.load() == 50);
}

// An exception thrown inside a task must not kill the worker thread; it is
// captured by the packaged_task and re-thrown at future::get().
void test_exception_propagates() {
    ThreadPool pool(2);
    auto bad = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    bool caught = false;
    try {
        bad.get();
    } catch (const std::runtime_error& e) {
        caught = (std::string(e.what()) == "boom");
    }
    assert(caught);

    // Pool is still healthy afterwards.
    assert(pool.submit([] { return 7; }).get() == 7);
}

// Queued work must still complete when shutdown drains.
void test_shutdown_drains() {
    std::atomic<int> ran{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 200; ++i) {
            pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
        }
    }  // destructor -> shutdown() -> drain + join
    assert(ran.load() == 200);
}

// Tasks that submit more tasks must not deadlock (lock is not held during call).
void test_nested_submit() {
    ThreadPool pool(4);
    auto outer = pool.submit([&pool] {
        auto inner = pool.submit([] { return 21; });
        return inner.get() * 2;
    });
    assert(outer.get() == 42);
}

void test_move_only_task() {
    auto owned = std::make_unique<int>(5);
    MoveOnlyTask t([p = std::move(owned)] { assert(*p == 5); });
    t();
}

int main() {
    test_returns_values();
    test_arguments_and_void();
    test_exception_propagates();
    test_shutdown_drains();
    test_nested_submit();
    test_move_only_task();
    std::cout << "thread_pool: ok\n";
    return 0;
}
