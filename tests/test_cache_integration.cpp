#include <gtest/gtest.h>
#include "libs/common/cache_manager.hpp"
#include "libs/common/cached_portfolio_manager.hpp"
#include "libs/common/service_host.hpp"
#include "libs/common/thread_pool.hpp"
#include <memory>
#include <chrono>
#include <thread>

class CacheIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        host = std::make_unique<ServiceHost>("cache_test_service");
        pool = std::make_unique<ThreadPool>(4);
        manager = std::make_unique<CacheManager>(host.get(), pool.get());
    }

    void TearDown() override {
        manager.reset();
        pool.reset();
        host.reset();
    }

    std::unique_ptr<ServiceHost> host;
    std::unique_ptr<ThreadPool> pool;
    std::unique_ptr<CacheManager> manager;
};

// Test basic cache manager functionality
TEST_F(CacheIntegrationTest, CreateAndManageCaches) {
    auto string_cache = manager->create_cache<std::string, int>("test_cache", 100);
    
    EXPECT_NE(string_cache, nullptr);
    EXPECT_EQ(string_cache->max_size(), 100);
    EXPECT_TRUE(string_cache->empty());
    
    // Test cache operations
    string_cache->put("key1", 42);
    EXPECT_EQ(string_cache->get("key1").value_or(-1), 42);
    
    // Get statistics through manager
    std::string stats = manager->get_all_statistics();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("test_cache"), std::string::npos);
}

TEST_F(CacheIntegrationTest, MultipleCachesManagement) {
    auto cache1 = manager->create_cache<std::string, int>("cache1", 50);
    auto cache2 = manager->create_cache<int, std::string>("cache2", 75);
    auto cache3 = manager->create_cache<std::string, double>("cache3", 100);
    
    // Populate caches
    cache1->put("key1", 100);
    cache2->put(1, "value1");
    cache3->put("pi", 3.14159);
    
    // Test statistics for all caches
    std::string all_stats = manager->get_all_statistics();
    EXPECT_NE(all_stats.find("cache1"), std::string::npos);
    EXPECT_NE(all_stats.find("cache2"), std::string::npos);
    EXPECT_NE(all_stats.find("cache3"), std::string::npos);
    
    // Test cleanup all
    manager->cleanup_all_caches();
    
    // Caches should still contain data (cleanup only removes expired items)
    EXPECT_EQ(cache1->get("key1").value_or(-1), 100);
    EXPECT_EQ(cache2->get(1).value_or(""), "value1");
    EXPECT_NEAR(cache3->get("pi").value_or(0.0), 3.14159, 0.0001);
}

TEST_F(CacheIntegrationTest, DistributedCacheCreation) {
    manager->enable_distributed_mode();
    
    auto distributed_cache = manager->create_distributed_cache<std::string, int>("distributed_test", 200);
    
    EXPECT_NE(distributed_cache, nullptr);
    EXPECT_EQ(distributed_cache->max_size(), 200);
    
    // Test that it works like a regular cache
    distributed_cache->put("dist_key", 999);
    EXPECT_EQ(distributed_cache->get("dist_key").value_or(-1), 999);
}

TEST_F(CacheIntegrationTest, TTLIntegration) {
    auto ttl_cache = manager->create_cache<std::string, int>("ttl_test", 100, 
                                                            std::chrono::milliseconds(100));
    
    ttl_cache->put("temp_key", 123);
    EXPECT_EQ(ttl_cache->get("temp_key").value_or(-1), 123);
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    EXPECT_FALSE(ttl_cache->get("temp_key").has_value());
}

// Test cached portfolio manager integration
class CachedPortfolioManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: CachedPortfolioManager creates its own ServiceHost and ThreadPool
        manager = std::make_unique<CachedPortfolioManager>("test_portfolio_service");
    }

    void TearDown() override {
        manager.reset();
    }

    std::unique_ptr<CachedPortfolioManager> manager;
};

TEST_F(CachedPortfolioManagerTest, CacheStatistics) {
    // Test that cache statistics are available
    manager->print_cache_statistics();
    
    auto portfolio_stats = manager->get_portfolio_cache_stats();
    auto market_stats = manager->get_market_cache_stats();
    auto calc_stats = manager->get_calculation_cache_stats();
    
    EXPECT_EQ(portfolio_stats.size, 0); // Initially empty
    EXPECT_EQ(market_stats.size, 0);
    EXPECT_EQ(calc_stats.size, 0);
    
    EXPECT_GT(portfolio_stats.max_size, 0); // Should have configured max sizes
    EXPECT_GT(market_stats.max_size, 0);
    EXPECT_GT(calc_stats.max_size, 0);
}

TEST_F(CachedPortfolioManagerTest, CacheCleanup) {
    // Test cleanup functionality
    manager->cleanup_caches();
    
    // Should not throw and statistics should be accessible
    auto portfolio_stats = manager->get_portfolio_cache_stats();
    EXPECT_GE(portfolio_stats.size, 0);
}

// Test async cache operations
class AsyncCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<ThreadPool>(2);
        cache = std::make_shared<LRUCache<std::string, int>>(100);
        async_ops = std::make_unique<AsyncCacheOperations<std::string, int>>(cache, pool.get());
    }

    void TearDown() override {
        async_ops.reset();
        cache.reset();
        pool.reset();
    }

    std::unique_ptr<ThreadPool> pool;
    std::shared_ptr<LRUCache<std::string, int>> cache;
    std::unique_ptr<AsyncCacheOperations<std::string, int>> async_ops;
};

TEST_F(AsyncCacheTest, AsyncGet) {
    cache->put("async_key", 456);
    
    std::atomic<bool> callback_called{false};
    std::atomic<int> result_value{-1};
    
    async_ops->get_async("async_key", [&](std::optional<int> result) {
        if (result.has_value()) {
            result_value = result.value();
        }
        callback_called = true;
    });
    
    // Wait for async operation
    while (!callback_called) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(result_value.load(), 456);
}

TEST_F(AsyncCacheTest, AsyncPut) {
    std::atomic<bool> put_completed{false};
    
    async_ops->put_async("async_put_key", 789);
    
    // Give async operation time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Verify it was stored
    EXPECT_EQ(cache->get("async_put_key").value_or(-1), 789);
}

TEST_F(AsyncCacheTest, ComputeIfAbsent) {
    std::atomic<bool> callback_called{false};
    std::atomic<int> computed_value{-1};
    
    async_ops->compute_if_absent_async(
        "compute_key",
        []() -> int {
            // Expensive computation simulation
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return 999;
        },
        [&](int value) {
            computed_value = value;
            callback_called = true;
        }
    );
    
    // Wait for computation and callback
    while (!callback_called) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(computed_value.load(), 999);
    
    // Verify it was cached
    EXPECT_EQ(cache->get("compute_key").value_or(-1), 999);
    
    // Test cache hit path
    callback_called = false;
    computed_value = -1;
    
    async_ops->compute_if_absent_async(
        "compute_key",
        []() -> int {
            return 888; // Should not be called
        },
        [&](int value) {
            computed_value = value;
            callback_called = true;
        }
    );
    
    while (!callback_called) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(computed_value.load(), 999); // Should return cached value
}

// Integration test with ServiceHost messaging
class CacheMessagingTest : public ::testing::Test {
protected:
    void SetUp() override {
        host = std::make_unique<ServiceHost>("cache_messaging_test");
        pool = std::make_unique<ThreadPool>(4);
        manager = std::make_unique<CacheManager>(host.get(), pool.get());
        
        // Enable distributed caching
        manager->enable_distributed_mode();
        
        cache = manager->create_distributed_cache<std::string, std::string>("messaging_cache", 100);
        
        // Set up cache statistics request handler
        cache_stats_received = false;
        host->subscribe("cache.stats.response", [this](const std::string& stats) {
            received_stats = stats;
            cache_stats_received = true;
        });
    }

    void TearDown() override {
        cache.reset();
        manager.reset();
        pool.reset();
        host.reset();
    }

    std::unique_ptr<ServiceHost> host;
    std::unique_ptr<ThreadPool> pool;
    std::unique_ptr<CacheManager> manager;
    std::shared_ptr<LRUCache<std::string, std::string>> cache;
    
    std::atomic<bool> cache_stats_received{false};
    std::string received_stats;
};

TEST_F(CacheMessagingTest, CacheStatsMessaging) {
    // Add some data to cache
    cache->put("msg_key1", "value1");
    cache->put("msg_key2", "value2");
    cache->get("msg_key1"); // Generate a hit
    cache->get("non_existent"); // Generate a miss
    
    // Request cache statistics via messaging
    host->publish_broadcast("cache.stats", "");
    
    // Wait for response
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!cache_stats_received && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(cache_stats_received);
    EXPECT_FALSE(received_stats.empty());
    EXPECT_NE(received_stats.find("messaging_cache"), std::string::npos);
}

// Performance integration test
TEST_F(CacheIntegrationTest, HighThroughputIntegration) {
    auto perf_cache = manager->create_cache<int, std::string>("perf_test", 1000);
    
    const int num_operations = 10000;
    const int num_threads = 4;
    
    std::atomic<int> operations_completed{0};
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Launch worker threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 500);
            
            for (int i = 0; i < num_operations / num_threads; ++i) {
                int key = dis(gen);
                std::string value = "thread_" + std::to_string(t) + "_value_" + std::to_string(i);
                
                if (i % 3 == 0) {
                    perf_cache->put(key, value);
                } else {
                    perf_cache->get(key);
                }
                
                operations_completed.fetch_add(1);
            }
        });
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_EQ(operations_completed.load(), num_operations);
    
    auto stats = perf_cache->get_statistics();
    double throughput = static_cast<double>(num_operations) / duration.count() * 1000; // ops/sec
    
    std::cout << "High Throughput Test Results:" << std::endl;
    std::cout << "Operations: " << num_operations << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " ops/sec" << std::endl;
    std::cout << "Hit Rate: " << (stats.hit_rate * 100) << "%" << std::endl;
    
    // Should achieve reasonable throughput
    EXPECT_GT(throughput, 10000); // At least 10K ops/sec
}

// Distributed cache simulation test
TEST_F(CacheIntegrationTest, DistributedCacheSimulation) {
    manager->enable_distributed_mode();
    
    auto dist_cache = manager->create_distributed_cache<std::string, int>("distributed_sim", 50);
    
    // Simulate distributed cache operations
    dist_cache->put("dist_item1", 111);
    dist_cache->put("dist_item2", 222);
    
    // Verify local cache functionality
    EXPECT_EQ(dist_cache->get("dist_item1").value_or(-1), 111);
    EXPECT_EQ(dist_cache->get("dist_item2").value_or(-1), 222);
    
    // Test cache invalidation simulation
    dist_cache->remove("dist_item1");
    EXPECT_FALSE(dist_cache->get("dist_item1").has_value());
    EXPECT_EQ(dist_cache->get("dist_item2").value_or(-1), 222);
}
