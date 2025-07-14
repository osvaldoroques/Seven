#include <gtest/gtest.h>
#include "libs/common/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test basic ThreadPool functionality
TEST_F(ThreadPoolTest, BasicFunctionality) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    
    // Submit 100 tasks
    for (int i = 0; i < 100; ++i) {
        bool submitted = pool.submit([&counter]() {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        EXPECT_TRUE(submitted);
    }
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(counter.load(), 100);
    EXPECT_EQ(pool.size(), 4);
    EXPECT_FALSE(pool.is_shutdown());
}

// Test ThreadPool with different thread counts
TEST_F(ThreadPoolTest, DifferentThreadCounts) {
    // Test with 1 thread
    {
        ThreadPool pool(1);
        EXPECT_EQ(pool.size(), 1);
    }
    
    // Test with 0 threads (should default to 1)
    {
        ThreadPool pool(0);
        EXPECT_EQ(pool.size(), 1);
    }
    
    // Test with many threads
    {
        ThreadPool pool(std::thread::hardware_concurrency() * 2);
        EXPECT_EQ(pool.size(), std::thread::hardware_concurrency() * 2);
    }
}

// Test shutdown behavior
TEST_F(ThreadPoolTest, ShutdownBehavior) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    
    // Submit some tasks
    for (int i = 0; i < 10; ++i) {
        bool submitted = pool.submit([&counter]() {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        EXPECT_TRUE(submitted);
    }
    
    // Shutdown the pool
    pool.shutdown();
    EXPECT_TRUE(pool.is_shutdown());
    
    // Try to submit after shutdown (should fail)
    bool submitted = pool.submit([&counter]() {
        counter.fetch_add(1);
    });
    EXPECT_FALSE(submitted);
    
    // Counter should still have processed some tasks
    EXPECT_GT(counter.load(), 0);
}

// Test exception safety in tasks
TEST_F(ThreadPoolTest, ExceptionSafety) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    
    // Submit tasks that throw exceptions
    for (int i = 0; i < 10; ++i) {
        bool submitted = pool.submit([&counter, i]() {
            if (i % 2 == 0) {
                throw std::runtime_error("Test exception");
            }
            counter.fetch_add(1);
        });
        EXPECT_TRUE(submitted);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should have processed 5 tasks (odd numbers)
    EXPECT_EQ(counter.load(), 5);
    EXPECT_FALSE(pool.is_shutdown());
}

// Test move semantics
TEST_F(ThreadPoolTest, MoveSemantics) {
    auto create_pool = []() {
        ThreadPool pool(2);
        auto counter = std::make_shared<std::atomic<int>>(0);
        
        for (int i = 0; i < 5; ++i) {
            pool.submit([counter]() {
                counter->fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
        }
        
        // Wait for tasks to complete before moving
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return pool;
    };
    
    ThreadPool moved_pool = create_pool();
    EXPECT_EQ(moved_pool.size(), 2);
    EXPECT_FALSE(moved_pool.is_shutdown());
}

// Test copy semantics are disabled
TEST_F(ThreadPoolTest, CopyDisabled) {
    // These should not compile
    static_assert(!std::is_copy_constructible_v<ThreadPool>);
    static_assert(!std::is_copy_assignable_v<ThreadPool>);
    
    // But move should be enabled
    static_assert(std::is_move_constructible_v<ThreadPool>);
    static_assert(std::is_move_assignable_v<ThreadPool>);
}

// Test concurrent submissions
TEST_F(ThreadPoolTest, ConcurrentSubmissions) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::thread> submitters;
    
    // Create multiple threads submitting tasks
    for (int t = 0; t < 4; ++t) {
        submitters.emplace_back([&pool, &counter]() {
            for (int i = 0; i < 25; ++i) {
                bool submitted = pool.submit([&counter]() {
                    counter.fetch_add(1);
                });
                EXPECT_TRUE(submitted);
            }
        });
    }
    
    // Wait for all submitters to finish
    for (auto& t : submitters) {
        t.join();
    }
    
    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(counter.load(), 100);
}

// Test pending tasks counter
TEST_F(ThreadPoolTest, PendingTasksCounter) {
    ThreadPool pool(1); // Single thread to create backlog
    std::atomic<int> counter{0};
    
    // Submit many tasks that take time
    for (int i = 0; i < 20; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    // Check that we have pending tasks
    size_t pending = pool.pending_tasks();
    EXPECT_GT(pending, 0);
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Should have no pending tasks now
    EXPECT_EQ(pool.pending_tasks(), 0);
    EXPECT_EQ(counter.load(), 20);
}

// Test task with different callable types
TEST_F(ThreadPoolTest, DifferentCallableTypes) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    
    // Lambda
    pool.submit([&counter]() { counter.fetch_add(1); });
    
    // Function pointer
    auto func = [](std::atomic<int>& c) { c.fetch_add(1); };
    pool.submit([&counter, func]() { func(counter); });
    
    // Callable object
    struct Callable {
        std::atomic<int>& counter;
        Callable(std::atomic<int>& c) : counter(c) {}
        void operator()() { counter.fetch_add(1); }
    };
    pool.submit(Callable(counter));
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(counter.load(), 3);
}

// Stress test with many tasks
TEST_F(ThreadPoolTest, StressTest) {
    ThreadPool pool(std::thread::hardware_concurrency());
    std::atomic<size_t> counter{0};
    const size_t num_tasks = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Submit many tasks
    for (size_t i = 0; i < num_tasks; ++i) {
        bool submitted = pool.submit([&counter]() {
            counter.fetch_add(1);
            // Simulate some work
            volatile int x = 0;
            for (int j = 0; j < 100; ++j) {
                x += j;
            }
        });
        EXPECT_TRUE(submitted);
    }
    
    // Wait for completion
    while (counter.load() < num_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(counter.load(), num_tasks);
    std::cout << "Processed " << num_tasks << " tasks in " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (num_tasks * 1000.0 / duration.count()) << " tasks/sec" << std::endl;
}

// Test RAII behavior
TEST_F(ThreadPoolTest, RAIIBehavior) {
    std::atomic<int> counter{0};
    
    {
        ThreadPool pool(2);
        
        // Submit long-running tasks
        for (int i = 0; i < 10; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
        }
        
        // Pool will be destroyed here, should wait for tasks to complete
    }
    
    // All tasks should have completed
    EXPECT_EQ(counter.load(), 10);
}
