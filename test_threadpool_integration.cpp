#include "libs/common/thread_pool.hpp"
#include "libs/common/service_host.hpp"
#include <iostream>
#include <chrono>
#include <atomic>

class ThreadPoolPerformanceTest {
private:
    ServiceHost host;
    ThreadPool pool;
    std::atomic<size_t> messages_processed{0};
    std::atomic<size_t> total_latency_ns{0};
    
public:
    ThreadPoolPerformanceTest() : host("test_service"), pool(4) {
        host.enable_tracing(true);
        
        // Set up message handler that uses thread pool
        host.subscribe("test.performance", [this](const std::string& message) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Submit work to thread pool
            bool submitted = pool.submit([this, start, message]() {
                // Simulate some processing
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
                auto end = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
                
                messages_processed.fetch_add(1);
                total_latency_ns.fetch_add(latency.count());
            });
            
            if (!submitted) {
                std::cout << "Failed to submit task to thread pool\n";
            }
        });
    }
    
    void run_test(size_t num_messages) {
        std::cout << "Starting ThreadPool + ServiceHost performance test...\n";
        std::cout << "Thread pool size: " << pool.size() << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Send messages
        for (size_t i = 0; i < num_messages; ++i) {
            host.publish_broadcast("test.performance", "test_message_" + std::to_string(i));
        }
        
        // Wait for processing to complete
        while (messages_processed.load() < num_messages) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Print results
        std::cout << "Messages processed: " << messages_processed.load() << std::endl;
        std::cout << "Total time: " << total_time.count() << " ms" << std::endl;
        std::cout << "Throughput: " << (messages_processed.load() * 1000.0 / total_time.count()) << " msg/sec" << std::endl;
        std::cout << "Average latency: " << (total_latency_ns.load() / messages_processed.load() / 1000) << " μs" << std::endl;
        std::cout << "Pending tasks: " << pool.pending_tasks() << std::endl;
    }
    
    ~ThreadPoolPerformanceTest() {
        pool.shutdown();
        std::cout << "ThreadPool shut down successfully\n";
    }
};

int main() {
    try {
        ThreadPoolPerformanceTest test;
        test.run_test(1000);
        
        std::cout << "Performance test completed successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
