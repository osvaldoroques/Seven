#pragma once
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <functional>
#include <chrono>
#include <atomic>

template<typename Key, typename Value>
class LRUCache {
public:
    using KeyType = Key;
    using ValueType = Value;
    using TimePoint = std::chrono::steady_clock::time_point;
    
private:
    struct CacheEntry {
        Value value;
        TimePoint access_time;
        TimePoint expiry_time;
        
        CacheEntry(Value v, TimePoint access, TimePoint expiry = TimePoint::max())
            : value(std::move(v)), access_time(access), expiry_time(expiry) {}
    };
    
    using ListIterator = typename std::list<std::pair<Key, CacheEntry>>::iterator;
    
    // Cache storage
    std::list<std::pair<Key, CacheEntry>> cache_list_;
    std::unordered_map<Key, ListIterator> cache_map_;
    
    // Configuration
    size_t max_size_;
    std::chrono::milliseconds default_ttl_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Statistics
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
    mutable std::atomic<size_t> evictions_{0};
    mutable std::atomic<size_t> expirations_{0};
    
    // Move item to front of the list (most recently used)
    void move_to_front(ListIterator it) {
        cache_list_.splice(cache_list_.begin(), cache_list_, it);
    }
    
    // Remove expired entries
    void cleanup_expired() {
        auto now = std::chrono::steady_clock::now();
        auto it = cache_list_.rbegin();
        
        while (it != cache_list_.rend()) {
            if (it->second.expiry_time <= now) {
                auto to_erase = std::next(it).base();
                --it;
                cache_map_.erase(to_erase->first);
                cache_list_.erase(to_erase);
                expirations_.fetch_add(1);
            } else {
                ++it;
            }
        }
    }
    
    // Evict least recently used item
    void evict_lru() {
        if (!cache_list_.empty()) {
            auto last = cache_list_.end();
            --last;
            cache_map_.erase(last->first);
            cache_list_.erase(last);
            evictions_.fetch_add(1);
        }
    }

public:
    explicit LRUCache(size_t max_size, 
                     std::chrono::milliseconds default_ttl = std::chrono::milliseconds::max())
        : max_size_(max_size), default_ttl_(default_ttl) {
        if (max_size_ == 0) {
            throw std::invalid_argument("Cache size must be greater than 0");
        }
    }
    
    // Disable copy constructor and assignment
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    
    // Enable move constructor and assignment
    LRUCache(LRUCache&& other) noexcept
        : cache_list_(std::move(other.cache_list_)),
          cache_map_(std::move(other.cache_map_)),
          max_size_(other.max_size_),
          default_ttl_(other.default_ttl_),
          hits_(other.hits_.load()),
          misses_(other.misses_.load()),
          evictions_(other.evictions_.load()),
          expirations_(other.expirations_.load()) {}
    
    LRUCache& operator=(LRUCache&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_list_ = std::move(other.cache_list_);
            cache_map_ = std::move(other.cache_map_);
            max_size_ = other.max_size_;
            default_ttl_ = other.default_ttl_;
            hits_ = other.hits_.load();
            misses_ = other.misses_.load();
            evictions_ = other.evictions_.load();
            expirations_ = other.expirations_.load();
        }
        return *this;
    }
    
    // Get value from cache
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            misses_.fetch_add(1);
            return std::nullopt;
        }
        
        auto list_it = it->second;
        auto now = std::chrono::steady_clock::now();
        
        // Check if expired
        if (list_it->second.expiry_time <= now) {
            cache_map_.erase(it);
            cache_list_.erase(list_it);
            expirations_.fetch_add(1);
            misses_.fetch_add(1);
            return std::nullopt;
        }
        
        // Update access time and move to front
        list_it->second.access_time = now;
        move_to_front(list_it);
        hits_.fetch_add(1);
        
        return list_it->second.value;
    }
    
    // Put value into cache
    void put(const Key& key, Value value, 
             std::chrono::milliseconds ttl = std::chrono::milliseconds::max()) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto expiry = (ttl == std::chrono::milliseconds::max()) 
                     ? TimePoint::max() 
                     : now + (ttl == std::chrono::milliseconds::max() ? default_ttl_ : ttl);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            auto list_it = it->second;
            list_it->second.value = std::move(value);
            list_it->second.access_time = now;
            list_it->second.expiry_time = expiry;
            move_to_front(list_it);
        } else {
            // Add new entry
            if (cache_list_.size() >= max_size_) {
                cleanup_expired();
                if (cache_list_.size() >= max_size_) {
                    evict_lru();
                }
            }
            
            cache_list_.emplace_front(key, CacheEntry(std::move(value), now, expiry));
            cache_map_[key] = cache_list_.begin();
        }
    }
    
    // Put value with custom TTL
    void put(const Key& key, Value value, std::chrono::seconds ttl_seconds) {
        put(key, std::move(value), std::chrono::duration_cast<std::chrono::milliseconds>(ttl_seconds));
    }
    
    // Remove specific key
    bool remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        cache_list_.erase(it->second);
        cache_map_.erase(it);
        return true;
    }
    
    // Clear all entries
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_list_.clear();
        cache_map_.clear();
    }
    
    // Get cache statistics
    struct Statistics {
        size_t size;
        size_t max_size;
        size_t hits;
        size_t misses;
        size_t evictions;
        size_t expirations;
        double hit_rate;
        double miss_rate;
    };
    
    Statistics get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t total_requests = hits_.load() + misses_.load();
        
        return Statistics{
            .size = cache_list_.size(),
            .max_size = max_size_,
            .hits = hits_.load(),
            .misses = misses_.load(),
            .evictions = evictions_.load(),
            .expirations = expirations_.load(),
            .hit_rate = total_requests > 0 ? static_cast<double>(hits_.load()) / total_requests : 0.0,
            .miss_rate = total_requests > 0 ? static_cast<double>(misses_.load()) / total_requests : 0.0
        };
    }
    
    // Check if key exists (without updating access time)
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        // Check if expired
        auto now = std::chrono::steady_clock::now();
        return it->second->second.expiry_time > now;
    }
    
    // Get current size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_list_.size();
    }
    
    // Check if empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_list_.empty();
    }
    
    // Get maximum size
    size_t max_size() const {
        return max_size_;
    }
    
    // Set new maximum size
    void resize(size_t new_max_size) {
        if (new_max_size == 0) {
            throw std::invalid_argument("Cache size must be greater than 0");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = new_max_size;
        
        // Evict items if necessary
        while (cache_list_.size() > max_size_) {
            evict_lru();
        }
    }
    
    // Cleanup expired entries manually
    size_t cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t initial_size = cache_list_.size();
        cleanup_expired();
        return initial_size - cache_list_.size();
    }
    
    // Get all keys (for debugging/monitoring)
    std::vector<Key> get_keys() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Key> keys;
        keys.reserve(cache_list_.size());
        
        for (const auto& item : cache_list_) {
            keys.push_back(item.first);
        }
        
        return keys;
    }
};
