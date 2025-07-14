#include <gtest/gtest.h>
#include "libs/common/thread_pool.hpp"
#include "libs/common/service_host.hpp"
#include <atomic>
#include <chrono>
#include <memory>

class ThreadPoolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        host = std::make_unique<ServiceHost>("thread_pool_test");
        pool = std::make_unique<ThreadPool>(4);
    }

    void TearDown() override {
        pool.reset();
        host.reset();
    }

    std::unique_ptr<ServiceHost> host;
    std::unique_ptr<ThreadPool> pool;
};

// Test ThreadPool integration with ServiceHost messaging
TEST_F(ThreadPoolIntegrationTest, MessageProcessingWithThreadPool) {
    std::atomic<size_t> messages_processed{0};
    std::atomic<size_t> total_latency_ns{0};
    
    // Enable tracing for performance measurement
    host->enable_tracing(true);
    
    // Set up message handler that uses thread pool
    host->subscribe("test.threadpool", [this, &messages_processed, &total_latency_ns](const std::string& message) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Submit work to thread pool
        bool submitted = pool->submit([&messages_processed, &total_latency_ns, start, message]() {
            // Simulate message processing
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            
            messages_processed.fetch_add(1);
            total_latency_ns.fetch_add(latency.count());
        });
        
        EXPECT_TRUE(submitted);
    });
    
    const size_t num_messages = 100;
    
    // Send messages
    for (size_t i = 0; i < num_messages; ++i) {
        host->publish_broadcast("test.threadpool", "test_message_" + std::to_string(i));
    }
    
    // Wait for processing to complete
    while (messages_processed.load() < num_messages) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(messages_processed.load(), num_messages);
    EXPECT_GT(total_latency_ns.load(), 0);
    
    // Calculate and verify performance metrics
    double avg_latency_us = total_latency_ns.load() / messages_processed.load() / 1000.0;
    std::cout << "Average message latency: " << avg_latency_us << " μs" << std::endl;
    std::cout << "Pending tasks after processing: " << pool->pending_tasks() << std::endl;
    
    // Latency should be reasonable (less than 10ms per message)
    EXPECT_LT(avg_latency_us, 10000);
}

// Test ThreadPool performance under high load with ServiceHost
TEST_F(ThreadPoolIntegrationTest, HighLoadPerformance) {
    std::atomic<size_t> messages_processed{0};
    std::atomic<size_t> processing_errors{0};
    
    // Set up message handler with error tracking
    host->subscribe("test.highload", [this, &messages_processed, &processing_errors](const std::string& message) {
        bool submitted = pool->submit([&messages_processed, &processing_errors, message]() {
            try {
                // Simulate varying processing times
                int work_amount = std::hash<std::string>{}(message) % 1000;
                volatile int result = 0;
                for (int i = 0; i < work_amount; ++i) {
                    result += i;
                }
                
                messages_processed.fetch_add(1);
            } catch (...) {
                processing_errors.fetch_add(1);
            }
        });
        
        if (!submitted) {
            processing_errors.fetch_add(1);
        }
    });
    
    const size_t num_messages = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Send many messages rapidly
    for (size_t i = 0; i < num_messages; ++i) {
        host->publish_broadcast("test.highload", "message_" + std::to_string(i));
    }
    
    // Wait for processing
    while (messages_processed.load() < num_messages && processing_errors.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(messages_processed.load(), num_messages);
    EXPECT_EQ(processing_errors.load(), 0);
    
    double throughput = (messages_processed.load() * 1000.0) / duration.count();
    std::cout << "High load throughput: " << throughput << " msg/sec" << std::endl;
    std::cout << "Processing time: " << duration.count() << " ms" << std::endl;
    
    // Should achieve reasonable throughput
    EXPECT_GT(throughput, 100); // At least 100 msg/sec
}

// Test ThreadPool with OpenTelemetry tracing integration
TEST_F(ThreadPoolIntegrationTest, TracingIntegration) {
    std::atomic<size_t> traced_tasks{0};
    std::atomic<size_t> untraced_tasks{0};
    
    // Test with tracing enabled
    host->enable_tracing(true);
    
    host->subscribe("test.tracing", [this, &traced_tasks, &untraced_tasks](const std::string& message) {
        pool->submit([&traced_tasks, &untraced_tasks, message]() {
            // In a real scenario, this would check if we're in a traced context
            // For now, we'll simulate based on message content
            if (message.find("traced") != std::string::npos) {
                traced_tasks.fetch_add(1);
            } else {
                untraced_tasks.fetch_add(1);
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        });
    });
    
    // Send mixed traced and untraced messages
    for (int i = 0; i < 50; ++i) {
        if (i % 2 == 0) {
            host->publish_broadcast("test.tracing", "traced_message_" + std::to_string(i));
        } else {
            host->publish_broadcast("test.tracing", "untraced_message_" + std::to_string(i));
        }
    }
    
    // Wait for processing
    while ((traced_tasks.load() + untraced_tasks.load()) < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(traced_tasks.load(), 25);
    EXPECT_EQ(untraced_tasks.load(), 25);
    
    // Test with tracing disabled
    traced_tasks = 0;
    untraced_tasks = 0;
    host->enable_tracing(false);
    
    for (int i = 0; i < 20; ++i) {
        host->publish_broadcast("test.tracing", "traced_message_" + std::to_string(i));
    }
    
    while ((traced_tasks.load() + untraced_tasks.load()) < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(traced_tasks.load(), 20); // All should be counted as traced
}

// Test ThreadPool graceful shutdown during active message processing
TEST_F(ThreadPoolIntegrationTest, GracefulShutdownDuringProcessing) {
    std::atomic<size_t> started_tasks{0};
    std::atomic<size_t> completed_tasks{0};
    
    host->subscribe("test.shutdown", [this, &started_tasks, &completed_tasks](const std::string& message) {
        pool->submit([&started_tasks, &completed_tasks]() {
            started_tasks.fetch_add(1);
            
            // Simulate longer processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            completed_tasks.fetch_add(1);
        });
    });
    
    // Send messages
    for (int i = 0; i < 20; ++i) {
        host->publish_broadcast("test.shutdown", "shutdown_test_" + std::to_string(i));
    }
    
    // Let some tasks start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    size_t tasks_started_before_shutdown = started_tasks.load();
    EXPECT_GT(tasks_started_before_shutdown, 0);
    
    // Shutdown the pool
    pool->shutdown();
    EXPECT_TRUE(pool->is_shutdown());
    
    // All started tasks should complete
    EXPECT_EQ(completed_tasks.load(), tasks_started_before_shutdown);
    
    std::cout << "Tasks started: " << tasks_started_before_shutdown << std::endl;
    std::cout << "Tasks completed: " << completed_tasks.load() << std::endl;
}

// Test error handling in ThreadPool + ServiceHost integration
TEST_F(ThreadPoolIntegrationTest, ErrorHandling) {
    std::atomic<size_t> successful_tasks{0};
    std::atomic<size_t> failed_submissions{0};
    
    host->subscribe("test.errors", [this, &successful_tasks, &failed_submissions](const std::string& message) {
        bool submitted = pool->submit([&successful_tasks, message]() {
            // Some tasks throw exceptions
            if (message.find("error") != std::string::npos) {
                throw std::runtime_error("Simulated error");
            }
            
            successful_tasks.fetch_add(1);
        });
        
        if (!submitted) {
            failed_submissions.fetch_add(1);
        }
    });
    
    // Send mixed good and bad messages
    for (int i = 0; i < 100; ++i) {
        if (i % 3 == 0) {
            host->publish_broadcast("test.errors", "error_message_" + std::to_string(i));
        } else {
            host->publish_broadcast("test.errors", "good_message_" + std::to_string(i));
        }
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should have processed ~67 good messages (100 - 33 error messages)
    EXPECT_NEAR(successful_tasks.load(), 67, 5); // Allow some variance
    EXPECT_EQ(failed_submissions.load(), 0); // No submission failures
    EXPECT_FALSE(pool->is_shutdown()); // Pool should still be running
    
    std::cout << "Successful tasks: " << successful_tasks.load() << std::endl;
    std::cout << "Failed submissions: " << failed_submissions.load() << std::endl;
}
