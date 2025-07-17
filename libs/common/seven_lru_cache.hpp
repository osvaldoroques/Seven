#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <mutex>
#include <chrono>

namespace seven {

/**
 * @brief Thread-safe LRU (Least Recently Used) cache implementation
 * 
 * This is a header-only, self-contained LRU cache that provides:
 * - O(1) get, put, and erase operations
 * - Thread-safe operations with minimal locking
 * - Optional TTL (Time-To-Live) support
 * - Statistics tracking
 * - Move semantics for efficiency
 * 
 * @tparam Key The type of keys stored in the cache
 * @tparam Value The type of values stored in the cache
 */
template<typename Key, typename Value>
class lru_cache {
public:
    struct Stats {
        size_t hits = 0;
        size_t misses = 0;
        size_t evictions = 0;
        
        double hit_rate() const {
            auto total = hits + misses;
            return total > 0 ? static_cast<double>(hits) / total : 0.0;
        }
    };

private:
    struct CacheNode {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point expiry_time;
        bool has_expiry;
        
        CacheNode(Key k, Value v) 
            : key(std::move(k)), value(std::move(v)), has_expiry(false) {}
            
        CacheNode(Key k, Value v, std::chrono::steady_clock::time_point exp) 
            : key(std::move(k)), value(std::move(v)), expiry_time(exp), has_expiry(true) {}
            
        bool is_expired() const {
            return has_expiry && std::chrono::steady_clock::now() > expiry_time;
        }
    };

    using NodeList = std::list<CacheNode>;
    using NodeIterator = typename NodeList::iterator;
    using Map = std::unordered_map<Key, NodeIterator>;

    size_t max_size_;
    std::chrono::seconds default_ttl_;
    bool use_ttl_;
    
    mutable std::mutex mutex_;
    NodeList nodes_;
    Map map_;
    mutable Stats stats_;

    void move_to_front(NodeIterator it) {
        nodes_.splice(nodes_.begin(), nodes_, it);
    }

    void evict_lru() {
        if (!nodes_.empty()) {
            auto last = nodes_.end();
            --last;
            map_.erase(last->key);
            nodes_.erase(last);
            ++stats_.evictions;
        }
    }

    void cleanup_expired_internal() {
        auto now = std::chrono::steady_clock::now();
        auto it = nodes_.begin();
        
        while (it != nodes_.end()) {
            if (it->has_expiry && now > it->expiry_time) {
                map_.erase(it->key);
                it = nodes_.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
    /**
     * @brief Construct a new LRU cache
     * @param max_size Maximum number of elements in the cache
     * @param ttl Default time-to-live for cache entries (0 = no expiry)
     */
    explicit lru_cache(size_t max_size, std::chrono::seconds ttl = std::chrono::seconds(0))
        : max_size_(max_size), default_ttl_(ttl), use_ttl_(ttl.count() > 0) {
        if (max_size_ == 0) {
            throw std::invalid_argument("Cache size must be greater than 0");
        }
    }

    /**
     * @brief Get a value from the cache
     * @param key The key to look up
     * @return Optional containing the value if found, nullopt otherwise
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto map_it = map_.find(key);
        if (map_it == map_.end()) {
            ++stats_.misses;
            return std::nullopt;
        }

        auto node_it = map_it->second;
        
        // Check if expired
        if (node_it->is_expired()) {
            map_.erase(map_it);
            nodes_.erase(node_it);
            ++stats_.misses;
            return std::nullopt;
        }

        // Move to front (most recently used)
        move_to_front(node_it);
        ++stats_.hits;
        return node_it->value;
    }

    /**
     * @brief Put a value into the cache
     * @param key The key to store
     * @param value The value to store
     */
    void put(Key key, Value value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto map_it = map_.find(key);
        
        if (map_it != map_.end()) {
            // Update existing entry
            auto node_it = map_it->second;
            node_it->value = std::move(value);
            
            // Update expiry time if using TTL
            if (use_ttl_) {
                node_it->expiry_time = std::chrono::steady_clock::now() + default_ttl_;
                node_it->has_expiry = true;
            }
            
            move_to_front(node_it);
            return;
        }

        // Add new entry
        if (nodes_.size() >= max_size_) {
            evict_lru();
        }

        // Create new node with optional TTL
        if (use_ttl_) {
            auto expiry = std::chrono::steady_clock::now() + default_ttl_;
            nodes_.emplace_front(key, std::move(value), expiry);
        } else {
            nodes_.emplace_front(key, std::move(value));
        }

        map_[std::move(key)] = nodes_.begin();
    }

    /**
     * @brief Check if a key exists in the cache
     * @param key The key to check
     * @return true if the key exists and is not expired, false otherwise
     */
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto map_it = map_.find(key);
        if (map_it == map_.end()) {
            return false;
        }

        auto node_it = map_it->second;
        return !node_it->is_expired();
    }

    /**
     * @brief Remove a specific key from the cache
     * @param key The key to remove
     * @return true if the key was found and removed, false otherwise
     */
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto map_it = map_.find(key);
        if (map_it == map_.end()) {
            return false;
        }

        auto node_it = map_it->second;
        map_.erase(map_it);
        nodes_.erase(node_it);
        return true;
    }

    /**
     * @brief Clear all entries from the cache
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        map_.clear();
        nodes_.clear();
        stats_ = Stats{};
    }

    /**
     * @brief Get the current number of entries in the cache
     * @return Number of entries
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodes_.size();
    }

    /**
     * @brief Get the maximum capacity of the cache
     * @return Maximum number of entries
     */
    size_t max_size() const {
        return max_size_;
    }

    /**
     * @brief Check if the cache is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodes_.empty();
    }

    /**
     * @brief Remove all expired entries from the cache
     */
    void cleanup_expired() {
        if (!use_ttl_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        cleanup_expired_internal();
    }

    /**
     * @brief Get cache statistics
     * @return Stats structure with hit/miss/eviction counts
     */
    Stats get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    /**
     * @brief Reset cache statistics
     */
    void reset_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = Stats{};
    }
};

} // namespace seven
