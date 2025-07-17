#include <iostream>
#include <chrono>
#include <thread>
#include "service_host.hpp"

// Example service that uses the integrated caching system
class CachedDataService {
private:
    ServiceHost* host_;
    
    // Cache instances for different data types
    std::shared_ptr<ServiceCache::CacheInstance<std::string, std::string>> user_cache_;
    std::shared_ptr<ServiceCache::CacheInstance<int, std::vector<std::string>>> product_cache_;
    std::shared_ptr<ServiceCache::CacheInstance<std::string, double>> price_cache_;

public:
    CachedDataService(ServiceHost* host) : host_(host) {
        // Create different cache instances through ServiceHost
        user_cache_ = host_->create_cache<std::string, std::string>(
            "user-cache", 1000, std::chrono::minutes(30)
        );
        
        product_cache_ = host_->create_cache<int, std::vector<std::string>>(
            "product-cache", 500, std::chrono::hours(1)
        );
        
        price_cache_ = host_->create_cache<std::string, double>(
            "price-cache", 2000, std::chrono::minutes(5)
        );
        
        std::cout << "✅ CachedDataService initialized with integrated caching\n";
    }

    void Register(ServiceHost* host) {
        // Service registration would go here
        std::cout << "🔌 CachedDataService registered with ServiceHost\n";
    }

    // Example: User data with caching
    std::string get_user_profile(const std::string& user_id) {
        // Check cache first
        auto cached_profile = user_cache_->get(user_id);
        if (cached_profile.has_value()) {
            std::cout << "🎯 Cache HIT for user: " << user_id << "\n";
            return *cached_profile;
        }
        
        // Simulate expensive database lookup
        std::cout << "💾 Cache MISS for user: " << user_id << " - fetching from database\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::string profile = "Profile data for user: " + user_id;
        
        // Cache the result
        user_cache_->put(user_id, profile);
        
        return profile;
    }

    // Example: Product catalog with caching
    std::vector<std::string> get_product_features(int product_id) {
        auto cached_features = product_cache_->get(product_id);
        if (cached_features.has_value()) {
            std::cout << "🎯 Cache HIT for product: " << product_id << "\n";
            return *cached_features;
        }
        
        std::cout << "💾 Cache MISS for product: " << product_id << " - computing features\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::vector<std::string> features = {
            "Feature A for product " + std::to_string(product_id),
            "Feature B for product " + std::to_string(product_id),
            "Feature C for product " + std::to_string(product_id)
        };
        
        product_cache_->put(product_id, features);
        
        return features;
    }

    // Example: Real-time pricing with short TTL
    double get_current_price(const std::string& symbol) {
        auto cached_price = price_cache_->get(symbol);
        if (cached_price.has_value()) {
            std::cout << "🎯 Cache HIT for price: " << symbol << "\n";
            return *cached_price;
        }
        
        std::cout << "💾 Cache MISS for price: " << symbol << " - fetching live price\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        
        // Simulate price calculation
        double price = 100.0 + (rand() % 5000) / 100.0;
        
        price_cache_->put(symbol, price);
        
        return price;
    }

    void print_cache_statistics() {
        std::cout << "\n📊 Cache Statistics:\n";
        
        auto user_stats = user_cache_->get_stats();
        std::cout << "User Cache: " << user_stats.size << "/" << user_stats.max_size 
                  << " (hit rate: " << (user_stats.hit_rate * 100) << "%)\n";
        
        auto product_stats = product_cache_->get_stats();
        std::cout << "Product Cache: " << product_stats.size << "/" << product_stats.max_size 
                  << " (hit rate: " << (product_stats.hit_rate * 100) << "%)\n";
        
        auto price_stats = price_cache_->get_stats();
        std::cout << "Price Cache: " << price_stats.size << "/" << price_stats.max_size 
                  << " (hit rate: " << (price_stats.hit_rate * 100) << "%)\n";
    }
};

int main() {
    std::cout << "🚀 ServiceHost Cache Integration Demo\n";
    std::cout << "=====================================\n\n";

    try {
        // Create ServiceHost with integrated caching
        CachedDataService service(nullptr);  // Would normally pass ServiceHost instance
        ServiceHost host("demo-uid", "cache-demo-service", service);
        
        // Initialize NATS connection (which also initializes cache system)
        host.init_nats();
        
        // Get the service instance (normally would be done differently)
        CachedDataService demo_service(&host);
        
        std::cout << "\n🔥 Running cache performance demo...\n\n";
        
        // Demo 1: User profile caching
        std::cout << "--- User Profile Cache Demo ---\n";
        for (int i = 0; i < 5; ++i) {
            auto profile = demo_service.get_user_profile("user123");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Demo 2: Product features caching
        std::cout << "\n--- Product Features Cache Demo ---\n";
        for (int i = 0; i < 3; ++i) {
            auto features = demo_service.get_product_features(456);
            std::cout << "Product features count: " << features.size() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Demo 3: Real-time pricing with TTL
        std::cout << "\n--- Real-time Price Cache Demo ---\n";
        for (int i = 0; i < 4; ++i) {
            auto price = demo_service.get_current_price("AAPL");
            std::cout << "AAPL price: $" << price << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Demo 4: Cache statistics
        demo_service.print_cache_statistics();
        
        // Demo 5: ServiceHost cache management features
        std::cout << "\n--- ServiceHost Cache Management ---\n";
        auto& service_cache = host.get_cache();
        
        // Get overall statistics
        auto all_stats = service_cache.get_all_stats();
        std::cout << "Total cache instances: " << all_stats.size() << "\n";
        
        // Test distributed cache operations (would work with actual NATS)
        std::cout << "Cache management endpoints available for:\n";
        std::cout << "  - cache.stats (get statistics)\n";
        std::cout << "  - cache.cleanup (cleanup expired entries)\n";
        std::cout << "  - cache.clear (clear cache contents)\n";
        std::cout << "  - cache.info (get cache information)\n";
        
        // Demo 6: Cache retrieval by name
        std::cout << "\n--- Cache Retrieval by Name ---\n";
        auto retrieved_cache = host.get_cache_instance<std::string, std::string>("user-cache");
        if (retrieved_cache) {
            std::cout << "✅ Successfully retrieved user-cache by name\n";
            std::cout << "Cache size: " << retrieved_cache->size() << "\n";
        }
        
        std::cout << "\n🎉 Cache integration demo completed successfully!\n";
        std::cout << "💡 All services now have automatic access to LRU caching through ServiceHost\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
