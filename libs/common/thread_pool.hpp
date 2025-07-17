#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool
{
public:
    ThreadPool(size_t n = std::thread::hardware_concurrency()) : done(false)
    {
        if (n == 0)
            n = 1;
        for (size_t i = 0; i < n; ++i)
            workers.emplace_back([this]
                                 { this->worker(); });
    }

    // Disable copy constructor and assignment
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Enable move constructor and assignment
    ThreadPool(ThreadPool&& other) noexcept 
        : workers(std::move(other.workers)),
          tasks(std::move(other.tasks)),
          mtx(),  // Cannot move mutex
          cv(),   // Cannot move condition_variable
          done(other.done.load())
    {
        other.done = true; // Mark other as done
    }
    
    ThreadPool& operator=(ThreadPool&& other) noexcept
    {
        if (this != &other) {
            shutdown(); // Shutdown current pool
            workers = std::move(other.workers);
            tasks = std::move(other.tasks);
            done = other.done.load();
            other.done = true;
        }
        return *this;
    }

    ~ThreadPool() noexcept
    {
        shutdown();
    }

    void shutdown() noexcept
    {
        if (done.exchange(true))
            return; // Already shut down

        try
        {
            // Wake up all workers to check the done flag
            cv.notify_all();

            for (auto &t : workers)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }
        }
        catch (...)
        {
            // Suppress all exceptions
        }
    }
    template <typename T>
    bool submit(T &&task)
    {
        {
            std::unique_lock<std::mutex> lk(mtx);
            if (done) {
                return false; // Don't accept new tasks after shutdown
            }
            tasks.push(std::function<void()>(std::forward<T>(task)));
            cv.notify_one();
            return true;
        }
    }

    // Utility methods
    size_t size() const { return workers.size(); }
    
    bool is_shutdown() const { return done.load(); }
    
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lk(mtx);
        return tasks.size();
    }
    
    size_t active_threads() const {
        // Return number of worker threads (all threads are considered active)
        return workers.size();
    }

private:
    // Worker loop: waits for tasks or shutdown signal.
    // The wait predicate 'done || !tasks.empty()' is safe because:
    // - If 'done' is true and the queue is empty, the worker exits.
    // - If there are tasks, the worker processes them.
    // - Spurious wakeups are handled by re-checking the predicate.
    void worker()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&]
                        { return done || !tasks.empty(); });
                if (done && tasks.empty())
                    break;
                // Simplified: if we're here and not done, there must be tasks
                task = std::move(tasks.front());
                tasks.pop();
            }
            // Execute task with exception safety
            try {
                task();
            } catch (...) {
                // Log error in production, but don't let exceptions kill the worker
                // Could integrate with your logging framework here
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> done;
};
