#pragma once

#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <chrono>

namespace tl {

template<typename Key, typename Value>
class lru_cache {
public:
    using key_type = Key;
    using value_type = Value;
    using size_type = std::size_t;
    
private:
    struct CacheItem {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point timestamp;
        
        CacheItem(const Key& k, const Value& v) 
            : key(k), value(v), timestamp(std::chrono::steady_clock::now()) {}
    };
    
    using ListIterator = typename std::list<CacheItem>::iterator;
    
    mutable std::mutex mutex_;
    std::list<CacheItem> items_;
    std::unordered_map<Key, ListIterator> cache_map_;
    size_type max_size_;
    std::chrono::seconds ttl_;
    
    void move_to_front(ListIterator it) {
        it->timestamp = std::chrono::steady_clock::now();
        items_.splice(items_.begin(), items_, it);
    }
    
    void evict_if_needed() {
        while (items_.size() > max_size_) {
            auto last = std::prev(items_.end());
            cache_map_.erase(last->key);
            items_.erase(last);
        }
    }
    
    bool is_expired(const CacheItem& item) const {
        if (ttl_.count() == 0) return false;
        auto now = std::chrono::steady_clock::now();
        return (now - item.timestamp) > ttl_;
    }
    
public:
    explicit lru_cache(size_type max_size, std::chrono::seconds ttl = std::chrono::seconds(0))
        : max_size_(max_size), ttl_(ttl) {}
    
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing item
            it->second->value = value;
            move_to_front(it->second);
            return;
        }
        
        // Add new item
        items_.emplace_front(key, value);
        cache_map_[key] = items_.begin();
        
        evict_if_needed();
    }
    
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return std::nullopt;
        }
        
        // Check if expired
        if (is_expired(*it->second)) {
            items_.erase(it->second);
            cache_map_.erase(it);
            return std::nullopt;
        }
        
        move_to_front(it->second);
        return it->second->value;
    }
    
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        
        // Check if expired
        if (is_expired(*it->second)) {
            // Note: We can't modify in a const method, so we just return false
            // The expired item will be cleaned up on next access
            return false;
        }
        
        return true;
    }
    
    void erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            items_.erase(it->second);
            cache_map_.erase(it);
        }
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
        cache_map_.clear();
    }
    
    size_type size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }
    
    size_type max_size() const {
        return max_size_;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.empty();
    }
    
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (ttl_.count() == 0) return;
        
        auto now = std::chrono::steady_clock::now();
        auto it = items_.begin();
        
        while (it != items_.end()) {
            if ((now - it->timestamp) > ttl_) {
                cache_map_.erase(it->key);
                it = items_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

} // namespace tl
