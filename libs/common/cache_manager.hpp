#pragma once
#include "lru_cache.hpp"
#include "service_host.hpp"
#include "thread_pool.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <type_traits>
#include <sstream>

// Serialization helpers
template<typename T>
struct CacheSerializer {
    static std::string serialize(const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            // For complex types, you might want to use JSON or protobuf
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    }
    
    static T deserialize(const std::string& data) {
        if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (std::is_integral_v<T>) {
                return static_cast<T>(std::stoll(data));
            } else {
                return static_cast<T>(std::stod(data));
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            return data;
        } else {
            // For complex types, implement custom deserialization
            T value;
            std::istringstream iss(data);
            iss >> value;
            return value;
        }
    }
};

class CacheManager {
private:
    struct CacheInfo {
        std::string name;
        size_t max_size;
        std::chrono::milliseconds default_ttl;
        std::function<void()> cleanup_func;
        std::function<std::string()> stats_func;
    };
    
    ServiceHost* service_host_;
    ThreadPool* thread_pool_;
    std::unordered_map<std::string, CacheInfo> cache_registry_;
    mutable std::mutex registry_mutex_;
    
    // Distributed cache coordination
    std::string cache_topic_prefix_ = "cache.";
    bool distributed_mode_ = false;
    
public:
    explicit CacheManager(ServiceHost* host = nullptr, ThreadPool* pool = nullptr)
        : service_host_(host), thread_pool_(pool) {
        
        if (service_host_) {
            setup_distributed_cache();
        }
    }
    
    // Create a new cache instance
    template<typename Key, typename Value>
    std::shared_ptr<LRUCache<Key, Value>> create_cache(
        const std::string& name,
        size_t max_size,
        std::chrono::milliseconds default_ttl = std::chrono::milliseconds::max()) {
        
        auto cache = std::make_shared<LRUCache<Key, Value>>(max_size, default_ttl);
        
        // Register the cache for monitoring and management
        {
            std::lock_guard<std::mutex> lock(registry_mutex_);
            cache_registry_[name] = CacheInfo{
                .name = name,
                .max_size = max_size,
                .default_ttl = default_ttl,
                .cleanup_func = [cache]() { cache->cleanup(); },
                .stats_func = [cache]() -> std::string {
                    auto stats = cache->get_statistics();
                    std::ostringstream oss;
                    oss << "size:" << stats.size 
                        << ",max_size:" << stats.max_size
                        << ",hits:" << stats.hits
                        << ",misses:" << stats.misses
                        << ",hit_rate:" << std::fixed << std::setprecision(2) << stats.hit_rate * 100 << "%"
                        << ",evictions:" << stats.evictions
                        << ",expirations:" << stats.expirations;
                    return oss.str();
                }
            };
        }
        
        return cache;
    }
    
    // Create a distributed cache that syncs across services
    template<typename Key, typename Value>
    std::shared_ptr<LRUCache<Key, Value>> create_distributed_cache(
        const std::string& name,
        size_t max_size,
        std::chrono::milliseconds default_ttl = std::chrono::milliseconds::max()) {
        
        auto cache = create_cache<Key, Value>(name, max_size, default_ttl);
        
        if (service_host_ && distributed_mode_) {
            setup_cache_synchronization<Key, Value>(name, cache);
        }
        
        return cache;
    }
    
    // Get cache statistics for all caches
    std::string get_all_statistics() const {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        std::ostringstream oss;
        
        oss << "Cache Statistics:\n";
        oss << "================\n";
        
        for (const auto& [name, info] : cache_registry_) {
            oss << "Cache: " << name << "\n";
            oss << "  " << info.stats_func() << "\n";
        }
        
        return oss.str();
    }
    
    // Cleanup all caches
    void cleanup_all_caches() {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        
        for (const auto& [name, info] : cache_registry_) {
            info.cleanup_func();
        }
    }
    
    // Enable distributed caching
    void enable_distributed_mode() {
        if (service_host_) {
            distributed_mode_ = true;
            setup_distributed_cache();
        }
    }
    
    // Disable distributed caching
    void disable_distributed_mode() {
        distributed_mode_ = false;
    }
    
private:
    void setup_distributed_cache() {
        if (!service_host_) return;
        
        // Subscribe to cache invalidation messages
        service_host_->subscribe(cache_topic_prefix_ + "invalidate", 
            [this](const std::string& message) {
                handle_cache_invalidation(message);
            });
        
        // Subscribe to cache statistics requests
        service_host_->subscribe(cache_topic_prefix_ + "stats", 
            [this](const std::string& message) {
                auto stats = get_all_statistics();
                service_host_->publish_broadcast(cache_topic_prefix_ + "stats.response", stats);
            });
    }
    
    template<typename Key, typename Value>
    void setup_cache_synchronization(const std::string& cache_name, 
                                   std::shared_ptr<LRUCache<Key, Value>> cache) {
        
        std::string update_topic = cache_topic_prefix_ + cache_name + ".update";
        std::string invalidate_topic = cache_topic_prefix_ + cache_name + ".invalidate";
        
        // Subscribe to cache updates from other services
        service_host_->subscribe(update_topic, 
            [cache](const std::string& message) {
                try {
                    // Parse message: "key:serialized_value:ttl_ms"
                    auto first_colon = message.find(':');
                    auto second_colon = message.find(':', first_colon + 1);
                    
                    if (first_colon != std::string::npos && second_colon != std::string::npos) {
                        std::string key_str = message.substr(0, first_colon);
                        std::string value_str = message.substr(first_colon + 1, second_colon - first_colon - 1);
                        std::string ttl_str = message.substr(second_colon + 1);
                        
                        Key key = CacheSerializer<Key>::deserialize(key_str);
                        Value value = CacheSerializer<Value>::deserialize(value_str);
                        auto ttl = std::chrono::milliseconds(std::stoll(ttl_str));
                        
                        cache->put(key, std::move(value), ttl);
                    }
                } catch (const std::exception& e) {
                    // Log error in production
                }
            });
        
        // Subscribe to cache invalidations
        service_host_->subscribe(invalidate_topic,
            [cache](const std::string& message) {
                try {
                    if (message == "*") {
                        cache->clear();
                    } else {
                        Key key = CacheSerializer<Key>::deserialize(message);
                        cache->remove(key);
                    }
                } catch (const std::exception& e) {
                    // Log error in production
                }
            });
    }
    
    void handle_cache_invalidation(const std::string& message) {
        // Handle global cache invalidation messages
        if (message == "cleanup_all") {
            cleanup_all_caches();
        }
    }
};

// Utility class for async cache operations
template<typename Key, typename Value>
class AsyncCacheOperations {
private:
    std::shared_ptr<LRUCache<Key, Value>> cache_;
    ThreadPool* thread_pool_;
    
public:
    AsyncCacheOperations(std::shared_ptr<LRUCache<Key, Value>> cache, ThreadPool* pool)
        : cache_(cache), thread_pool_(pool) {}
    
    // Async get with callback
    void get_async(const Key& key, std::function<void(std::optional<Value>)> callback) {
        if (thread_pool_) {
            thread_pool_->submit([this, key, callback = std::move(callback)]() {
                auto result = cache_->get(key);
                callback(result);
            });
        } else {
            // Fallback to synchronous operation
            auto result = cache_->get(key);
            callback(result);
        }
    }
    
    // Async put
    void put_async(Key key, Value value, std::chrono::milliseconds ttl = std::chrono::milliseconds::max()) {
        if (thread_pool_) {
            thread_pool_->submit([this, key = std::move(key), value = std::move(value), ttl]() {
                cache_->put(key, std::move(value), ttl);
            });
        } else {
            // Fallback to synchronous operation
            cache_->put(key, std::move(value), ttl);
        }
    }
    
    // Async compute if absent (get or compute and put)
    void compute_if_absent_async(
        const Key& key,
        std::function<Value()> value_factory,
        std::function<void(Value)> callback,
        std::chrono::milliseconds ttl = std::chrono::milliseconds::max()) {
        
        if (thread_pool_) {
            thread_pool_->submit([this, key, value_factory = std::move(value_factory), 
                                 callback = std::move(callback), ttl]() {
                auto existing = cache_->get(key);
                if (existing.has_value()) {
                    callback(existing.value());
                } else {
                    auto new_value = value_factory();
                    cache_->put(key, new_value, ttl);
                    callback(new_value);
                }
            });
        } else {
            // Fallback to synchronous operation
            auto existing = cache_->get(key);
            if (existing.has_value()) {
                callback(existing.value());
            } else {
                auto new_value = value_factory();
                cache_->put(key, new_value, ttl);
                callback(new_value);
            }
        }
    }
};
