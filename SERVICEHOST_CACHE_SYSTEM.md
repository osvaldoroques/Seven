# ServiceHost Integrated Caching System

## Overview

The Seven framework now includes a comprehensive LRU caching system directly integrated into the ServiceHost core. This means **all services automatically have access to high-performance, thread-safe caching capabilities** without any additional setup or dependencies.

## Key Features

- **🔌 Zero-Configuration**: Caching is wired directly into ServiceHost - available to all services immediately
- **📚 Proven Library**: Uses `tl::lru_cache` - a mature, header-only LRU cache implementation
- **🔒 Thread-Safe**: Full concurrent access support with mutex protection
- **⏰ TTL Support**: Time-based expiration with automatic cleanup
- **📊 Statistics**: Comprehensive metrics (hit rate, size, evictions, etc.)
- **🌐 Distributed**: NATS-based cache coordination and invalidation
- **🎯 Type-Safe**: Template-based with compile-time type checking
- **⚡ High Performance**: Optimized for hot-path operations

## Quick Start

### Basic Usage

```cpp
#include "service_host.hpp"

class MyService {
public:
    MyService(ServiceHost* host) : host_(host) {
        // Create a cache instance through ServiceHost
        my_cache_ = host_->create_cache<std::string, UserData>(
            "user-cache",           // Cache name
            1000,                   // Max size
            std::chrono::minutes(30) // TTL (optional)
        );
    }

    void Register(ServiceHost* host) {
        // Service registration logic
    }

    UserData get_user(const std::string& user_id) {
        // Check cache first
        auto cached = my_cache_->get(user_id);
        if (cached.has_value()) {
            return *cached;
        }

        // Fetch from database
        UserData user = fetch_from_database(user_id);
        
        // Cache the result
        my_cache_->put(user_id, user);
        
        return user;
    }

private:
    ServiceHost* host_;
    std::shared_ptr<ServiceCache::CacheInstance<std::string, UserData>> my_cache_;
};

// Use with ServiceHost
int main() {
    MyService service(nullptr);
    ServiceHost host("my-uid", "my-service", service);
    
    host.init_nats(); // This also initializes the cache system
    
    // All services now have caching capabilities!
    return 0;
}
```

### Multiple Cache Types

```cpp
class DataService {
private:
    std::shared_ptr<ServiceCache::CacheInstance<std::string, std::string>> string_cache_;
    std::shared_ptr<ServiceCache::CacheInstance<int, std::vector<double>>> vector_cache_;
    std::shared_ptr<ServiceCache::CacheInstance<UserId, UserProfile>> user_cache_;

public:
    DataService(ServiceHost* host) {
        // Different cache instances for different data types
        string_cache_ = host->create_cache<std::string, std::string>("strings", 500);
        vector_cache_ = host->create_cache<int, std::vector<double>>("vectors", 200);
        user_cache_ = host->create_cache<UserId, UserProfile>("users", 1000, std::chrono::hours(1));
    }
};
```

## API Reference

### ServiceHost Cache Methods

```cpp
// Create a new cache instance
template<typename Key, typename Value>
auto create_cache(const std::string& name, size_t max_size, 
                 std::chrono::seconds ttl = std::chrono::seconds(0));

// Retrieve existing cache by name
template<typename Key, typename Value>
auto get_cache_instance(const std::string& name);

// Get the ServiceCache manager
ServiceCache& get_cache();
const ServiceCache& get_cache() const;
```

### Cache Instance Operations

```cpp
// Basic operations
void put(const Key& key, const Value& value);
std::optional<Value> get(const Key& key);
bool contains(const Key& key) const;
void erase(const Key& key);
void clear();

// Capacity and size
size_t size() const;
size_t max_size() const;
bool empty() const;

// TTL operations (if TTL enabled)
void cleanup_expired();

// Statistics
CacheStats get_stats() const;
```

### CacheStats Structure

```cpp
struct CacheStats {
    std::string name;
    size_t size;
    size_t max_size;
    size_t hits;
    size_t misses;
    size_t evictions;
    double hit_rate;
};
```

## Advanced Features

### Time-To-Live (TTL) Support

```cpp
// Cache with 5-minute expiration
auto cache = host->create_cache<std::string, Data>(
    "temp-cache", 
    100, 
    std::chrono::minutes(5)
);

// Manual cleanup of expired entries
cache->cleanup_expired();
```

### Cache Statistics and Monitoring

```cpp
// Individual cache statistics
auto stats = cache->get_stats();
std::cout << "Hit rate: " << (stats.hit_rate * 100) << "%" << std::endl;
std::cout << "Size: " << stats.size << "/" << stats.max_size << std::endl;

// All cache statistics
auto& service_cache = host.get_cache();
auto all_stats = service_cache.get_all_stats();
for (const auto& stat : all_stats) {
    std::cout << stat.name << ": " << stat.size << " entries" << std::endl;
}
```

### Distributed Cache Management

The system provides NATS-based distributed cache coordination:

```cpp
// These endpoints are automatically available:
// cache.stats         - Get cache statistics
// cache.cleanup       - Cleanup expired entries
// cache.clear         - Clear cache contents
// cache.info          - Get cache information
// cache.invalidate    - Distributed invalidation
```

### Thread Safety

All cache operations are thread-safe:

```cpp
// Multiple threads can safely access the same cache
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&cache, i]() {
        for (int j = 0; j < 100; ++j) {
            cache->put(std::to_string(i * 100 + j), j);
            auto value = cache->get(std::to_string(i * 100 + j));
            // All operations are thread-safe
        }
    });
}
```

## Best Practices

### 1. Cache Naming Convention

Use descriptive, hierarchical names:

```cpp
auto user_cache = host->create_cache<UserId, User>("users.profiles", 1000);
auto session_cache = host->create_cache<SessionId, Session>("users.sessions", 5000);
auto product_cache = host->create_cache<ProductId, Product>("catalog.products", 2000);
```

### 2. Appropriate Cache Sizes

Size caches based on your use case:

```cpp
// User sessions - high turnover
auto sessions = host->create_cache<std::string, Session>("sessions", 10000, std::chrono::hours(24));

// Configuration data - low turnover
auto config = host->create_cache<std::string, Config>("config", 100, std::chrono::hours(1));

// Real-time data - very short TTL
auto prices = host->create_cache<std::string, Price>("prices", 1000, std::chrono::seconds(30));
```

### 3. Error Handling

Always check for cache misses:

```cpp
auto get_user_safe(const std::string& user_id) -> std::optional<User> {
    auto cached = user_cache_->get(user_id);
    if (cached.has_value()) {
        return cached;
    }
    
    try {
        auto user = database_->fetch_user(user_id);
        user_cache_->put(user_id, user);
        return user;
    } catch (const std::exception& e) {
        logger_->error("Failed to fetch user {}: {}", user_id, e.what());
        return std::nullopt;
    }
}
```

### 4. Cache Warming

Implement cache warming strategies:

```cpp
void warm_user_cache() {
    auto frequent_users = get_frequent_user_ids();
    for (const auto& user_id : frequent_users) {
        if (!user_cache_->contains(user_id)) {
            auto user = fetch_user(user_id);
            user_cache_->put(user_id, user);
        }
    }
}
```

## Performance Characteristics

- **Memory**: O(n) where n is the number of cached items
- **Access Time**: O(1) average case for get/put operations
- **Thread Contention**: Minimal due to efficient locking strategy
- **Cache Eviction**: LRU policy ensures optimal memory usage

## Integration with Seven Framework

The caching system is fully integrated with Seven's infrastructure:

- **Logging**: All cache operations are logged with correlation IDs
- **Configuration**: Cache settings can be managed through config files
- **Monitoring**: Cache metrics are available through ServiceHost endpoints
- **Distribution**: NATS integration for distributed cache coordination
- **Testing**: Comprehensive test suite ensures reliability

## Troubleshooting

### Common Issues

1. **Cache Not Found**: Ensure cache is created before retrieval
   ```cpp
   auto cache = host->get_cache_instance<Key, Value>("my-cache");
   if (!cache) {
       // Cache doesn't exist - create it first
       cache = host->create_cache<Key, Value>("my-cache", 100);
   }
   ```

2. **Type Mismatch**: Ensure consistent types when retrieving caches
   ```cpp
   // Wrong - different value types
   auto cache1 = host->create_cache<std::string, int>("test", 10);
   auto cache2 = host->get_cache_instance<std::string, double>("test"); // nullptr!
   
   // Correct - matching types
   auto cache1 = host->create_cache<std::string, int>("test", 10);
   auto cache2 = host->get_cache_instance<std::string, int>("test"); // works!
   ```

3. **Memory Usage**: Monitor cache statistics to prevent memory issues
   ```cpp
   auto stats = cache->get_stats();
   if (stats.size > stats.max_size * 0.9) {
       // Cache is nearly full - consider increasing size or reducing TTL
   }
   ```

## Testing

Comprehensive test coverage is provided:

```bash
# Run cache integration tests
cd build && ctest -R service_host_cache_test -V

# Run all tests including cache functionality
cd build && ctest -V
```

## Examples

See the following files for complete examples:

- `cache_integration_demo.cpp` - Complete usage demonstration
- `tests/test_service_host_cache.cpp` - Comprehensive test cases
- `services/portfolio_manager/` - Real-world integration example

---

**🎉 The ServiceHost caching system provides enterprise-grade caching capabilities directly built into the Seven framework, making high-performance caching available to all services with zero configuration!**
