#include "libs/common/thread_pool.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>

int main() {
    std::cout << "Testing ThreadPool with sanitizers...\n";
    
    // Test 1: Basic functionality
    {
        ThreadPool pool(4);
        std::atomic<int> counter{0};
        
        // Submit 100 tasks
        for (int i = 0; i < 100; ++i) {
            bool submitted = pool.submit([&counter, i]() {
                counter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
            if (!submitted) {
                std::cout << "Failed to submit task " << i << std::endl;
            }
        }
        
        // Wait a bit for tasks to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::cout << "Counter value: " << counter.load() << std::endl;
        std::cout << "Pool size: " << pool.size() << std::endl;
        std::cout << "Is shutdown: " << pool.is_shutdown() << std::endl;
    }
    
    // Test 2: Shutdown behavior
    {
        ThreadPool pool(2);
        std::atomic<int> counter{0};
        
        // Submit some tasks
        for (int i = 0; i < 10; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }
        
        // Shutdown immediately
        pool.shutdown();
        
        // Try to submit after shutdown (should fail)
        bool submitted = pool.submit([&counter]() {
            counter.fetch_add(1);
        });
        
        std::cout << "Submit after shutdown: " << (submitted ? "SUCCESS" : "FAILED (expected)") << std::endl;
        std::cout << "Final counter: " << counter.load() << std::endl;
    }
    
    // Test 3: Move semantics
    {
        auto create_pool = []() {
            ThreadPool pool(2);
            // Use a shared counter to avoid stack-use-after-scope
            auto counter = std::make_shared<std::atomic<int>>(0);
            
            for (int i = 0; i < 5; ++i) {
                pool.submit([counter]() {
                    counter->fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                });
            }
            
            // Wait for tasks to complete before moving
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return pool; // Move constructor
        };
        
        ThreadPool moved_pool = create_pool();
        std::cout << "Moved pool size: " << moved_pool.size() << std::endl;
    }
    
    std::cout << "ThreadPool tests completed!\n";
    return 0;
}
