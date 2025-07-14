#include <gtest/gtest.h>
#include "libs/common/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>
#include <random>

// This test is specifically designed to exercise potential race conditions
// and memory safety issues that sanitizers can detect
class ThreadPoolSanitizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator for stress testing
        rd = std::make_unique<std::random_device>();
        gen = std::make_unique<std::mt19937>((*rd)());
        dist = std::make_unique<std::uniform_int_distribution<>>(1, 100);
    }

    void TearDown() override {
        // Clean up
    }

    std::unique_ptr<std::random_device> rd;
    std::unique_ptr<std::mt19937> gen;
    std::unique_ptr<std::uniform_int_distribution<>> dist;
};

// Test for AddressSanitizer: Memory access patterns
TEST_F(ThreadPoolSanitizerTest, MemoryAccessPatterns) {
    ThreadPool pool(4);
    
    // Use shared data that multiple threads will access
    auto shared_data = std::make_shared<std::vector<std::atomic<int>>>(1000);
    for (auto& item : *shared_data) {
        item.store(0);
    }
    
    std::atomic<size_t> tasks_completed{0};
    
    // Submit tasks that access shared memory
    for (size_t i = 0; i < 500; ++i) {
        bool submitted = pool.submit([shared_data, &tasks_completed, i]() {
            // Access different parts of the shared vector
            size_t index = i % shared_data->size();
            (*shared_data)[index].fetch_add(1);
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            
            tasks_completed.fetch_add(1);
        });
        EXPECT_TRUE(submitted);
    }
    
    // Wait for completion
    while (tasks_completed.load() < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Verify all tasks completed
    EXPECT_EQ(tasks_completed.load(), 500);
    
    // Check that memory was accessed correctly
    size_t total_increments = 0;
    for (const auto& item : *shared_data) {
        total_increments += item.load();
    }
    EXPECT_EQ(total_increments, 500);
}

// Test for ThreadSanitizer: Data race detection
TEST_F(ThreadPoolSanitizerTest, DataRaceDetection) {
    ThreadPool pool(8);
    
    // Use proper atomic operations to avoid data races
    std::atomic<long long> shared_counter{0};
    std::atomic<size_t> read_operations{0};
    std::atomic<size_t> write_operations{0};
    
    // Submit mixed read/write tasks
    for (int i = 0; i < 1000; ++i) {
        if (i % 2 == 0) {
            // Write task
            pool.submit([&shared_counter, &write_operations]() {
                shared_counter.fetch_add(1);
                write_operations.fetch_add(1);
            });
        } else {
            // Read task
            pool.submit([&shared_counter, &read_operations]() {
                volatile long long value = shared_counter.load();
                (void)value; // Suppress unused variable warning
                read_operations.fetch_add(1);
            });
        }
    }
    
    // Wait for completion
    while ((read_operations.load() + write_operations.load()) < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_EQ(read_operations.load(), 500);
    EXPECT_EQ(write_operations.load(), 500);
    EXPECT_EQ(shared_counter.load(), 500); // Only write operations increment
}

// Test for UndefinedBehaviorSanitizer: Integer overflow detection
TEST_F(ThreadPoolSanitizerTest, IntegerOverflowPrevention) {
    ThreadPool pool(4);
    std::atomic<size_t> safe_operations{0};
    std::atomic<size_t> overflow_prevented{0};
    
    // Submit tasks that could potentially overflow
    for (int i = 0; i < 100; ++i) {
        pool.submit([&safe_operations, &overflow_prevented, i]() {
            // Safe arithmetic operations
            int base = 1000000;
            int multiplier = i + 1;
            
            // Check for potential overflow before multiplication
            if (base <= INT_MAX / multiplier) {
                int result = base * multiplier;
                (void)result; // Use the result
                safe_operations.fetch_add(1);
            } else {
                overflow_prevented.fetch_add(1);
            }
        });
    }
    
    // Wait for completion
    while ((safe_operations.load() + overflow_prevented.load()) < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_GT(safe_operations.load(), 0);
    // Some operations might be prevented based on INT_MAX value
}

// Test for memory initialization patterns
TEST_F(ThreadPoolSanitizerTest, MemoryInitialization) {
    ThreadPool pool(4);
    std::atomic<size_t> tasks_completed{0};
    
    // Submit tasks that properly initialize memory
    for (int i = 0; i < 200; ++i) {
        pool.submit([&tasks_completed]() {
            // Properly initialize all memory before use
            std::vector<int> local_data(100, 0); // Initialize to zero
            
            // Fill with known values
            for (size_t j = 0; j < local_data.size(); ++j) {
                local_data[j] = static_cast<int>(j);
            }
            
            // Use the data
            int sum = 0;
            for (int val : local_data) {
                sum += val;
            }
            
            // Verify expected sum
            int expected_sum = (100 * 99) / 2; // Sum of 0 to 99
            if (sum == expected_sum) {
                tasks_completed.fetch_add(1);
            }
        });
    }
    
    // Wait for completion
    while (tasks_completed.load() < 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_EQ(tasks_completed.load(), 200);
}

// Test rapid pool creation and destruction
TEST_F(ThreadPoolSanitizerTest, RapidPoolLifecycle) {
    std::atomic<size_t> pools_created{0};
    std::atomic<size_t> tasks_executed{0};
    
    // Create and destroy pools rapidly
    for (int iteration = 0; iteration < 50; ++iteration) {
        {
            ThreadPool pool(2);
            pools_created.fetch_add(1);
            
            // Submit a few tasks
            for (int i = 0; i < 5; ++i) {
                pool.submit([&tasks_executed]() {
                    tasks_executed.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                });
            }
            
            // Pool destructor will wait for tasks to complete
        }
    }
    
    EXPECT_EQ(pools_created.load(), 50);
    EXPECT_EQ(tasks_executed.load(), 250);
}

// Test concurrent pool operations
TEST_F(ThreadPoolSanitizerTest, ConcurrentPoolOperations) {
    ThreadPool pool(6);
    std::atomic<size_t> query_operations{0};
    std::atomic<size_t> submit_operations{0};
    std::atomic<size_t> work_completed{0};
    
    // Thread 1: Continuously query pool state
    std::thread query_thread([&pool, &query_operations]() {
        for (int i = 0; i < 1000; ++i) {
            size_t size = pool.size();
            bool shutdown = pool.is_shutdown();
            size_t pending = pool.pending_tasks();
            
            (void)size; (void)shutdown; (void)pending; // Suppress warnings
            query_operations.fetch_add(1);
            
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 2: Submit tasks
    std::thread submit_thread([&pool, &submit_operations, &work_completed]() {
        for (int i = 0; i < 500; ++i) {
            bool submitted = pool.submit([&work_completed]() {
                work_completed.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            });
            
            if (submitted) {
                submit_operations.fetch_add(1);
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });
    
    // Wait for threads to complete
    query_thread.join();
    submit_thread.join();
    
    // Wait for all work to complete
    while (work_completed.load() < submit_operations.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_EQ(query_operations.load(), 1000);
    EXPECT_GT(submit_operations.load(), 0);
    EXPECT_EQ(work_completed.load(), submit_operations.load());
}

// Test exception safety with sanitizers
TEST_F(ThreadPoolSanitizerTest, ExceptionSafetyWithSanitizers) {
    ThreadPool pool(4);
    std::atomic<size_t> successful_tasks{0};
    std::atomic<size_t> exception_tasks{0};
    
    // Submit tasks with various exception behaviors
    for (int i = 0; i < 300; ++i) {
        pool.submit([&successful_tasks, &exception_tasks, i]() {
            try {
                if (i % 7 == 0) {
                    // Throw different types of exceptions
                    if (i % 14 == 0) {
                        throw std::runtime_error("Runtime error");
                    } else {
                        throw std::logic_error("Logic error");
                    }
                }
                
                // Normal processing
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                successful_tasks.fetch_add(1);
                
            } catch (...) {
                exception_tasks.fetch_add(1);
                // Exception should be caught and not propagate
            }
        });
    }
    
    // Wait for completion
    while ((successful_tasks.load() + exception_tasks.load()) < 300) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should have ~257 successful tasks and ~43 exception tasks
    EXPECT_NEAR(successful_tasks.load(), 257, 10);
    EXPECT_NEAR(exception_tasks.load(), 43, 10);
    EXPECT_FALSE(pool.is_shutdown()); // Pool should survive exceptions
}

// Performance test to stress sanitizers
TEST_F(ThreadPoolSanitizerTest, SanitizerStressTest) {
    ThreadPool pool(std::thread::hardware_concurrency());
    std::atomic<size_t> operations_completed{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    const size_t num_operations = 5000;
    
    // Submit many small tasks to stress the sanitizers
    for (size_t i = 0; i < num_operations; ++i) {
        pool.submit([&operations_completed, i]() {
            // Perform various operations that sanitizers monitor
            
            // Memory allocation and deallocation
            auto data = std::make_unique<std::array<int, 100>>();
            data->fill(static_cast<int>(i % 1000));
            
            // Atomic operations
            std::atomic<int> local_atomic{static_cast<int>(i)};
            local_atomic.fetch_add(1);
            
            // Vector operations
            std::vector<int> vec;
            vec.reserve(50);
            for (int j = 0; j < 50; ++j) {
                vec.push_back(j);
            }
            
            operations_completed.fetch_add(1);
        });
    }
    
    // Wait for all operations to complete
    while (operations_completed.load() < num_operations) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(operations_completed.load(), num_operations);
    
    std::cout << "Sanitizer stress test completed " << num_operations 
              << " operations in " << duration.count() << " ms" << std::endl;
    std::cout << "Operations per second: " 
              << (num_operations * 1000.0 / duration.count()) << std::endl;
}
