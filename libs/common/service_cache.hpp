#pragma once
#include "seven_lru_cache.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <atomic>
#include <type_traits>
#include <sstream>
#include <iomanip>  // For std::setprecision

// Forward declaration for ServiceHost
class ServiceHost;

/**
 * ServiceCache - Integrated caching for ServiceHost
 * 
 * Provides automatic caching capabilities for all services:
 * - Message response caching
 * - Computation result caching  
 * - Database query caching
 * - Session data caching
 * 
 * Features:
 * - Thread-safe LRU eviction
 * - TTL (Time To Live) support
 * - Automatic cache warming
 * - Distributed cache invalidation via NATS
 * - Comprehensive metrics
 */
class ServiceCache {
public:
    // Cache configuration
    struct CacheConfig {
        size_t max_size = 1000;
        std::chrono::seconds ttl = std::chrono::seconds(3600); // 1 hour (renamed from default_ttl)
        bool distributed = false;
        std::string name;
    };
    
    // Cache statistics
    struct CacheStats {
        size_t size;
        size_t max_size;
        size_t hits;
        size_t misses;
        size_t evictions;
        double hit_rate;
        std::string name;
    };
    
private:
    // Internal cache entry with TTL
    template<typename T>
    struct CacheEntry {
        T value;
        std::chrono::steady_clock::time_point expiry;
        
        CacheEntry(T val, std::chrono::steady_clock::time_point exp)
            : value(std::move(val)), expiry(exp) {}
        
        bool is_expired() const {
            return std::chrono::steady_clock::now() >= expiry;
        }
    };
    
    // Type-erased cache interface for management
    class ICacheInstance {
    public:
        virtual ~ICacheInstance() = default;
        virtual void clear() = 0;
        virtual size_t size() const = 0;
        virtual size_t max_size() const = 0;
        virtual CacheStats get_stats() const = 0;
        virtual void cleanup_expired() = 0;
    };
    
    // Concrete cache implementation
    template<typename Key, typename Value>
    class CacheInstance : public ICacheInstance {
    private:
        seven::lru_cache<Key, Value> cache_;
        CacheConfig config_;
        mutable std::atomic<size_t> hits_{0};
        mutable std::atomic<size_t> misses_{0};
        mutable std::atomic<size_t> evictions_{0};  // Added missing evictions counter
        mutable std::mutex mutex_;  // Added missing mutex
        
    public:
        explicit CacheInstance(const CacheConfig& cfg) 
            : cache_(cfg.max_size, cfg.ttl), config_(cfg) {}
        
        std::optional<Value> get(const Key& key) {
            auto result = cache_.get(key);
            if (result.has_value()) {
                hits_.fetch_add(1);
                return result;
            } else {
                misses_.fetch_add(1);
                return std::nullopt;
            }
        }
        
        void put(const Key& key, Value value) {
            cache_.put(key, std::move(value));
        }
        
        bool contains(const Key& key) const {
            return cache_.contains(key);
        }
        
        bool erase(const Key& key) {
            return cache_.erase(key);
        }
        
        void clear() override {
            cache_.clear();
            hits_.store(0);
            misses_.store(0);
            evictions_.store(0);
        }
        
        size_t size() const override {
            return cache_.size();
        }
        
        size_t max_size() const override {
            return config_.max_size;
        }
        
        void cleanup_expired() override {
            std::lock_guard<std::mutex> lock(mutex_);
            // Get stats before cleanup to calculate items removed
            auto stats_before = cache_.get_stats();
            size_t size_before = cache_.size();
            
            // Perform cleanup
            cache_.cleanup_expired();
            
            // Calculate and track evictions from cleanup
            size_t size_after = cache_.size();
            size_t cleaned = size_before - size_after;
            evictions_.fetch_add(cleaned);
        }
        
        CacheStats get_stats() const override {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t total_requests = hits_.load() + misses_.load();
            
            return CacheStats{
                .size = cache_.size(),
                .max_size = config_.max_size,
                .hits = hits_.load(),
                .misses = misses_.load(),
                .evictions = evictions_.load(),
                .hit_rate = total_requests > 0 ? static_cast<double>(hits_.load()) / total_requests : 0.0,
                .name = config_.name
            };
        }
    };
    
    ServiceHost* host_;
    std::unordered_map<std::string, std::unique_ptr<ICacheInstance>> caches_;
    mutable std::mutex caches_mutex_;
    bool distributed_mode_ = false;
    
public:
    explicit ServiceCache(ServiceHost* host) : host_(host) {
        setup_cache_management();
    }
    
    // Create or get a typed cache
    template<typename Key, typename Value>
    std::shared_ptr<CacheInstance<Key, Value>> get_cache(
        const std::string& name,
        const CacheConfig& config = {}) {
        
        std::lock_guard<std::mutex> lock(caches_mutex_);
        
        auto it = caches_.find(name);
        if (it != caches_.end()) {
            // Return existing cache (type must match)
            auto cache_ptr = dynamic_cast<CacheInstance<Key, Value>*>(it->second.get());
            if (cache_ptr) {
                return std::shared_ptr<CacheInstance<Key, Value>>(
                    cache_ptr, [](CacheInstance<Key, Value>*) {
                        // Custom deleter that does nothing (managed by unique_ptr in map)
                    });
            }
            throw std::runtime_error("Cache exists with different type: " + name);
        }
        
        // Create new cache
        CacheConfig final_config = config;
        final_config.name = name;
        if (final_config.max_size == 0) final_config.max_size = 1000;
        
        auto cache = std::make_unique<CacheInstance<Key, Value>>(final_config);
        auto cache_ptr = cache.get();
        caches_[name] = std::move(cache);
        
        return std::shared_ptr<CacheInstance<Key, Value>>(
            cache_ptr, [](CacheInstance<Key, Value>*) {
                // Custom deleter that does nothing
            });
    }
    
    // Convenience methods for common cache operations
    template<typename Key, typename Value>
    std::optional<Value> get(const std::string& cache_name, const Key& key) {
        auto cache = get_cache<Key, Value>(cache_name);
        return cache->get(key);
    }
    
    template<typename Key, typename Value>
    void put(const std::string& cache_name, const Key& key, Value value,
             std::chrono::seconds ttl = std::chrono::seconds::max()) {
        auto cache = get_cache<Key, Value>(cache_name);
        cache->put(key, std::move(value), ttl);
    }
    
    template<typename Key, typename Value>
    bool remove(const std::string& cache_name, const Key& key) {
        auto cache = get_cache<Key, Value>(cache_name);
        return cache->remove(key);
    }
    
    // Cache management
    void clear_cache(const std::string& name) {
        std::lock_guard<std::mutex> lock(caches_mutex_);
        auto it = caches_.find(name);
        if (it != caches_.end()) {
            it->second->clear();
        }
    }
    
    void clear_all_caches() {
        std::lock_guard<std::mutex> lock(caches_mutex_);
        for (auto& [name, cache] : caches_) {
            cache->clear();
        }
    }
    
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(caches_mutex_);
        for (auto& [name, cache] : caches_) {
            cache->cleanup_expired();
        }
    }
    
    // Statistics and monitoring
    std::vector<CacheStats> get_all_stats() const {
        std::lock_guard<std::mutex> lock(caches_mutex_);
        std::vector<CacheStats> stats;
        stats.reserve(caches_.size());
        
        for (const auto& [name, cache] : caches_) {
            stats.push_back(cache->get_stats());
        }
        
        return stats;
    }
    
    std::string get_stats_summary() const {
        auto stats = get_all_stats();
        std::ostringstream oss;
        
        oss << "Cache Statistics Summary:\n";
        oss << "========================\n";
        
        for (const auto& stat : stats) {
            oss << "Cache: " << stat.name << "\n";
            oss << "  Size: " << stat.size << "/" << stat.max_size << "\n";
            oss << "  Hit Rate: " << std::fixed << std::setprecision(1) 
                << (stat.hit_rate * 100) << "%\n";
            oss << "  Hits: " << stat.hits << ", Misses: " << stat.misses << "\n";
            oss << "  Evictions: " << stat.evictions << "\n\n";
        }
        
        return oss.str();
    }
    
    // Enable distributed caching
    void enable_distributed_mode() {
        distributed_mode_ = true;
        setup_distributed_cache_handlers();
    }
    
    // Create a new cache instance (required by ServiceHost)
    template<typename Key, typename Value>
    std::shared_ptr<CacheInstance<Key, Value>> create_cache(
        const std::string& name,
        size_t max_size,
        std::chrono::seconds ttl = std::chrono::seconds(0)) {
        
        CacheConfig config;
        config.name = name;
        config.max_size = max_size;
        config.ttl = ttl;
        
        return get_cache<Key, Value>(name, config);
    }
    
    // Get existing cache instance by name and type (required by ServiceHost)
    template<typename Key, typename Value>
    std::shared_ptr<CacheInstance<Key, Value>> get_cache_instance(const std::string& name) {
        std::lock_guard<std::mutex> lock(caches_mutex_);
        
        auto it = caches_.find(name);
        if (it == caches_.end()) {
            return nullptr;
        }
        
        auto cache_ptr = dynamic_cast<CacheInstance<Key, Value>*>(it->second.get());
        if (!cache_ptr) {
            return nullptr; // Type mismatch
        }
        
        return std::shared_ptr<CacheInstance<Key, Value>>(
            cache_ptr, [](CacheInstance<Key, Value>*) {
                // Custom deleter that does nothing
            });
    }
    
    // Compute-if-absent pattern
    template<typename Key, typename Value>
    Value compute_if_absent(const std::string& cache_name, const Key& key,
                           std::function<Value()> compute_function,
                           std::chrono::seconds ttl = std::chrono::seconds::max()) {
        auto cache = get_cache<Key, Value>(cache_name);
        
        auto cached_value = cache->get(key);
        if (cached_value.has_value()) {
            return cached_value.value();
        }
        
        // Compute new value
        Value new_value = compute_function();
        cache->put(key, new_value, ttl);
        return new_value;
    }
    
    // Cache management and setup methods
    void setup_cache_management();
    void setup_distributed_cache_handlers();

private:
};
