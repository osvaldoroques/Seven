#include <gtest/gtest.h>
#include "libs/common/lru_cache.hpp"
#include "libs/common/cache_manager.hpp"
#include "libs/common/cached_portfolio_manager.hpp"
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

class LRUCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<LRUCache<std::string, int>>(3);
    }

    void TearDown() override {
        cache.reset();
    }

    std::unique_ptr<LRUCache<std::string, int>> cache;
};

// Basic functionality tests
TEST_F(LRUCacheTest, BasicPutAndGet) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    EXPECT_EQ(cache->get("key1").value_or(-1), 100);
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
    EXPECT_EQ(cache->get("key3").value_or(-1), 300);
    EXPECT_EQ(cache->size(), 3);
}

TEST_F(LRUCacheTest, LRUEviction) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    // This should evict key1 (least recently used)
    cache->put("key4", 400);
    
    EXPECT_FALSE(cache->get("key1").has_value()); // Evicted
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
    EXPECT_EQ(cache->get("key3").value_or(-1), 300);
    EXPECT_EQ(cache->get("key4").value_or(-1), 400);
    EXPECT_EQ(cache->size(), 3);
}

TEST_F(LRUCacheTest, AccessUpdatesOrder) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    // Access key1 to make it most recently used
    cache->get("key1");
    
    // Add new item - should evict key2 now
    cache->put("key4", 400);
    
    EXPECT_EQ(cache->get("key1").value_or(-1), 100); // Still present
    EXPECT_FALSE(cache->get("key2").has_value()); // Evicted
    EXPECT_EQ(cache->get("key3").value_or(-1), 300);
    EXPECT_EQ(cache->get("key4").value_or(-1), 400);
}

TEST_F(LRUCacheTest, UpdateExistingKey) {
    cache->put("key1", 100);
    cache->put("key1", 150); // Update
    
    EXPECT_EQ(cache->get("key1").value_or(-1), 150);
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(LRUCacheTest, RemoveKey) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    
    EXPECT_TRUE(cache->remove("key1"));
    EXPECT_FALSE(cache->remove("key1")); // Already removed
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(LRUCacheTest, Clear) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    cache->clear();
    
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
    EXPECT_FALSE(cache->get("key1").has_value());
}

TEST_F(LRUCacheTest, Contains) {
    cache->put("key1", 100);
    
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("key2"));
    
    cache->remove("key1");
    EXPECT_FALSE(cache->contains("key1"));
}

// TTL (Time To Live) tests
TEST_F(LRUCacheTest, TTLExpiration) {
    cache->put("key1", 100, std::chrono::milliseconds(50));
    
    EXPECT_EQ(cache->get("key1").value_or(-1), 100);
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_EQ(cache->size(), 0); // Expired items are cleaned up on access
}

TEST_F(LRUCacheTest, TTLWithSeconds) {
    cache->put("key1", 100, std::chrono::seconds(1));
    
    EXPECT_EQ(cache->get("key1").value_or(-1), 100);
    
    // Should still be valid after 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(cache->get("key1").value_or(-1), 100);
}

TEST_F(LRUCacheTest, ManualCleanup) {
    cache->put("key1", 100, std::chrono::milliseconds(50));
    cache->put("key2", 200); // No TTL
    
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    // Before cleanup
    EXPECT_EQ(cache->size(), 2);
    
    // Manual cleanup
    size_t cleaned = cache->cleanup();
    EXPECT_EQ(cleaned, 1); // One expired item cleaned
    EXPECT_EQ(cache->size(), 1);
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
}

// Statistics tests
TEST_F(LRUCacheTest, Statistics) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    
    // Generate some hits and misses
    cache->get("key1"); // Hit
    cache->get("key1"); // Hit
    cache->get("key3"); // Miss
    cache->get("key2"); // Hit
    cache->get("key4"); // Miss
    
    auto stats = cache->get_statistics();
    
    EXPECT_EQ(stats.size, 2);
    EXPECT_EQ(stats.max_size, 3);
    EXPECT_EQ(stats.hits, 3);
    EXPECT_EQ(stats.misses, 2);
    EXPECT_NEAR(stats.hit_rate, 0.6, 0.01); // 3/5 = 0.6
    EXPECT_NEAR(stats.miss_rate, 0.4, 0.01); // 2/5 = 0.4
}

TEST_F(LRUCacheTest, EvictionStatistics) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    cache->put("key4", 400); // Should trigger eviction
    
    auto stats = cache->get_statistics();
    EXPECT_EQ(stats.evictions, 1);
}

// Resize tests
TEST_F(LRUCacheTest, ResizeIncrease) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    cache->resize(5);
    EXPECT_EQ(cache->max_size(), 5);
    
    // Should be able to add more items without eviction
    cache->put("key4", 400);
    cache->put("key5", 500);
    
    EXPECT_EQ(cache->size(), 5);
    EXPECT_EQ(cache->get("key1").value_or(-1), 100); // Still present
}

TEST_F(LRUCacheTest, ResizeDecrease) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    cache->resize(2);
    EXPECT_EQ(cache->max_size(), 2);
    EXPECT_EQ(cache->size(), 2); // Should evict oldest item
    
    // key1 should be evicted (least recently used)
    EXPECT_FALSE(cache->get("key1").has_value());
    EXPECT_EQ(cache->get("key2").value_or(-1), 200);
    EXPECT_EQ(cache->get("key3").value_or(-1), 300);
}

TEST_F(LRUCacheTest, ResizeZeroThrows) {
    EXPECT_THROW(cache->resize(0), std::invalid_argument);
}

// Thread safety tests
TEST_F(LRUCacheTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int operations_per_thread = 1000;
    std::vector<std::thread> threads;
    
    // Resize cache for concurrent testing
    cache->resize(100);
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 50);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(dis(gen));
                int value = t * 1000 + i;
                
                if (i % 3 == 0) {
                    cache->put(key, value);
                } else {
                    cache->get(key);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Cache should be in valid state
    EXPECT_LE(cache->size(), cache->max_size());
    
    auto stats = cache->get_statistics();
    EXPECT_GT(stats.hits + stats.misses, 0);
}

// Move semantics tests
TEST_F(LRUCacheTest, MoveConstructor) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    
    LRUCache<std::string, int> moved_cache = std::move(*cache);
    
    EXPECT_EQ(moved_cache.get("key1").value_or(-1), 100);
    EXPECT_EQ(moved_cache.get("key2").value_or(-1), 200);
    EXPECT_EQ(moved_cache.size(), 2);
}

// Type safety tests
TEST_F(LRUCacheTest, DifferentTypes) {
    LRUCache<int, std::string> int_string_cache(5);
    LRUCache<std::string, std::vector<int>> string_vector_cache(3);
    
    int_string_cache.put(1, "one");
    int_string_cache.put(2, "two");
    
    EXPECT_EQ(int_string_cache.get(1).value_or(""), "one");
    EXPECT_EQ(int_string_cache.get(2).value_or(""), "two");
    
    string_vector_cache.put("numbers", {1, 2, 3, 4, 5});
    auto vec = string_vector_cache.get("numbers");
    EXPECT_TRUE(vec.has_value());
    EXPECT_EQ(vec.value().size(), 5);
}

// Performance test
TEST_F(LRUCacheTest, PerformanceStressTest) {
    const int cache_size = 1000;
    const int num_operations = 100000;
    
    LRUCache<int, int> perf_cache(cache_size);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, cache_size * 2); // 50% hit rate expected
    
    for (int i = 0; i < num_operations; ++i) {
        int key = dis(gen);
        
        if (i % 3 == 0) {
            perf_cache.put(key, key * 2);
        } else {
            perf_cache.get(key);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    auto stats = perf_cache.get_statistics();
    double ops_per_ms = static_cast<double>(num_operations) / duration.count();
    
    std::cout << "Performance Test Results:" << std::endl;
    std::cout << "Operations: " << num_operations << std::endl;
    std::cout << "Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Ops/ms: " << ops_per_ms << std::endl;
    std::cout << "Hit rate: " << (stats.hit_rate * 100) << "%" << std::endl;
    
    // Should be reasonably fast
    EXPECT_GT(ops_per_ms, 100); // At least 100 operations per millisecond
}

// Edge cases
TEST_F(LRUCacheTest, ZeroSizeThrows) {
    EXPECT_THROW(LRUCache<std::string, int>(0), std::invalid_argument);
}

TEST_F(LRUCacheTest, GetKeysDebug) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    auto keys = cache->get_keys();
    EXPECT_EQ(keys.size(), 3);
    
    // Keys should be in LRU order (most recent first)
    EXPECT_EQ(keys[0], "key3"); // Most recently inserted
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key1"); // Least recently used
}
