#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t n = std::thread::hardware_concurrency()) : done(false) {
        for(size_t i = 0; i < n; ++i)
            workers.emplace_back([this] { this->worker(); });
    }

    ~ThreadPool() {
        shutdown();
    }

    void shutdown() {
        if (done) return;  // Already shut down
        
        { 
            std::unique_lock<std::mutex> lk(mtx); 
            done = true; 
            cv.notify_all(); 
        }
        
        for(auto &t: workers) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lk(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

private:
    void worker() {
        while(true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [&]{return done || !tasks.empty();});
                if(done && tasks.empty()) break;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> done;
};
