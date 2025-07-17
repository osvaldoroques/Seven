#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "service_host.hpp"

class ServiceHostCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a ServiceHost with cache integration
        host = std::make_unique<ServiceHost>("test-uid", "test-service");
    }

    void TearDown() override {
        if (host) {
            host->shutdown();
        }
    }

    std::unique_ptr<ServiceHost> host;
};

TEST_F(ServiceHostCacheTest, CacheCreationAndAccess) {
    // Test cache creation through ServiceHost
    auto cache = host->create_cache<std::string, int>("test-cache", 10);
    ASSERT_NE(cache, nullptr);
    
    // Test basic cache operations
    cache->put("key1", 42);
    ASSERT_TRUE(cache->contains("key1"));
    
    auto value = cache->get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

TEST_F(ServiceHostCacheTest, MultipleCacheInstances) {
    // Create multiple cache instances
    auto string_cache = host->create_cache<std::string, std::string>("string-cache", 5);
    auto int_cache = host->create_cache<int, double>("int-cache", 10);
    
    ASSERT_NE(string_cache, nullptr);
    ASSERT_NE(int_cache, nullptr);
    
    // Test operations on different caches
    string_cache->put("hello", "world");
    int_cache->put(123, 3.14);
    
    EXPECT_TRUE(string_cache->contains("hello"));
    EXPECT_TRUE(int_cache->contains(123));
    
    auto str_val = string_cache->get("hello");
    auto int_val = int_cache->get(123);
    
    ASSERT_TRUE(str_val.has_value());
    ASSERT_TRUE(int_val.has_value());
    EXPECT_EQ(*str_val, "world");
    EXPECT_DOUBLE_EQ(*int_val, 3.14);
}

TEST_F(ServiceHostCacheTest, CacheWithTTL) {
    // Create cache with TTL
    auto cache = host->create_cache<std::string, int>("ttl-cache", 10, std::chrono::seconds(1));
    ASSERT_NE(cache, nullptr);
    
    // Add item and verify it exists
    cache->put("temp-key", 999);
    ASSERT_TRUE(cache->contains("temp-key"));
    
    // Wait for TTL expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Cleanup expired items
    cache->cleanup_expired();
    
    // Verify item has expired
    EXPECT_FALSE(cache->contains("temp-key"));
}

TEST_F(ServiceHostCacheTest, CacheStatistics) {
    auto cache = host->create_cache<std::string, int>("stats-cache", 5);
    ASSERT_NE(cache, nullptr);
    
    // Generate some cache activity
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->get("key1");  // Hit
    cache->get("key3");  // Miss
    
    auto stats = cache->get_stats();
    EXPECT_EQ(stats.name, "stats-cache");
    EXPECT_EQ(stats.size, 2);
    EXPECT_EQ(stats.max_size, 5);
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);
    EXPECT_DOUBLE_EQ(stats.hit_rate, 0.5);
}

TEST_F(ServiceHostCacheTest, CacheRetrievalByName) {
    // Create cache and put some data
    host->create_cache<std::string, int>("named-cache", 10);
    
    // Retrieve cache by name
    auto retrieved_cache = host->get_cache_instance<std::string, int>("named-cache");
    ASSERT_NE(retrieved_cache, nullptr);
    
    // Test operations on retrieved cache
    retrieved_cache->put("test", 123);
    EXPECT_TRUE(retrieved_cache->contains("test"));
    
    auto value = retrieved_cache->get("test");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 123);
}

TEST_F(ServiceHostCacheTest, CacheIntegrationWithServiceHost) {
    // Test that cache is properly integrated into ServiceHost
    auto& service_cache = host->get_cache();
    
    // Verify we can access cache functionality
    auto cache = service_cache.create_cache<std::string, std::string>("integration-test", 5);
    ASSERT_NE(cache, nullptr);
    
    cache->put("integrated", "yes");
    EXPECT_TRUE(cache->contains("integrated"));
    
    auto all_stats = service_cache.get_all_stats();
    EXPECT_GE(all_stats.size(), 1);
    
    bool found_cache = false;
    for (const auto& stats : all_stats) {
        if (stats.name == "integration-test") {
            found_cache = true;
            EXPECT_EQ(stats.size, 1);
            break;
        }
    }
    EXPECT_TRUE(found_cache);
}

TEST_F(ServiceHostCacheTest, ThreadSafeCacheOperations) {
    auto cache = host->create_cache<int, std::string>("thread-safe-cache", 100);
    ASSERT_NE(cache, nullptr);
    
    const int num_threads = 10;
    const int ops_per_thread = 50;
    std::vector<std::thread> threads;
    
    // Launch multiple threads performing cache operations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&cache, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                int key = i * ops_per_thread + j;
                cache->put(key, "value" + std::to_string(key));
                
                // Sometimes read the value back
                if (j % 3 == 0) {
                    auto value = cache->get(key);
                    // Value should exist since we just put it
                    EXPECT_TRUE(value.has_value());
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify cache contains expected number of items (up to max_size)
    auto stats = cache->get_stats();
    EXPECT_LE(stats.size, 100);  // Should not exceed max size
    EXPECT_GT(stats.size, 0);    // Should contain some items
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
