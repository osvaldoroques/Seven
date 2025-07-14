#include <catch2/catch.hpp>
#include <chrono>
#include <csignal>
#include <thread>
#include <fstream>
#include "service_host.hpp"
#include "messages_register.hpp"
#include "messages.pb.h"

class GracefulShutdownTest {
public:
    void SetUp() {
        // Create a test config file
        std::ofstream config_file("test_config.yaml");
        config_file << "nats:\n";
        config_file << "  url: nats://localhost:4222\n";
        config_file << "threads: 2\n";
        config_file.close();
    }
    
    void TearDown() {
        std::remove("test_config.yaml");
    }
};

TEST_CASE("ServiceHost Graceful Shutdown", "[graceful_shutdown]") {
    GracefulShutdownTest test;
    test.SetUp();
    
    // Create a ServiceHost instance
    auto svc = std::make_unique<ServiceHost>(
        "test-service-001",
        "test_config.yaml",
        MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint,
            [](const Trevor::HealthCheckRequest& req) {
                std::cout << "Test handler executed" << std::endl;
            })
    );
    
    REQUIRE(svc->is_running());
    
    // Test manual shutdown
    svc->stop();
    REQUIRE_FALSE(svc->is_running());
    
    // Shutdown should be idempotent
    svc->shutdown();
    svc->shutdown();  // Should not crash
    
    test.TearDown();
}

TEST_CASE("Thread Pool Shutdown", "[thread_pool]") {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    // Submit some tasks
    for (int i = 0; i < 5; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            counter++;
        });
    }
    
    // Allow some tasks to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Shutdown the pool
    pool.shutdown();
    
    // All tasks should have completed
    REQUIRE(counter.load() == 5);
}

TEST_CASE("Configuration Stop Watch", "[configuration]") {
    GracefulShutdownTest test;
    test.SetUp();
    
    Configuration config("test_config.yaml");
    
    bool callback_called = false;
    config.onReload([&callback_called]() {
        callback_called = true;
    });
    
    config.startWatch();
    
    // Modify the config file to trigger reload
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::ofstream config_file("test_config.yaml", std::ios::app);
    config_file << "new_setting: test\n";
    config_file.close();
    
    // Wait for file watcher to detect change
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    config.stopWatch();
    
    // The callback should have been called
    REQUIRE(callback_called);
    
    test.TearDown();
}
