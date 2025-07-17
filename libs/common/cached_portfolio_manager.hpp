#pragma once
#include "libs/common/lru_cache.hpp"
#include "libs/common/cache_manager.hpp"
#include "libs/common/service_host.hpp"
#include "libs/common/thread_pool.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <unordered_map>

// Example: Portfolio data structures for caching
struct PortfolioData {
    std::string portfolio_id;
    std::vector<std::string> holdings;
    double total_value;
    std::chrono::system_clock::time_point last_updated;
    
    // For serialization (if needed for distributed cache)
    friend std::ostream& operator<<(std::ostream& os, const PortfolioData& data) {
        os << data.portfolio_id << ";" << data.total_value << ";" 
           << data.holdings.size() << ";";
        for (const auto& holding : data.holdings) {
            os << holding << ",";
        }
        return os;
    }
    
    friend std::istream& operator>>(std::istream& is, PortfolioData& data) {
        std::string line;
        std::getline(is, line);
        // Simple parsing - in production use JSON or protobuf
        // This is just for demonstration
        return is;
    }
};

struct MarketData {
    std::string symbol;
    double price;
    double volume;
    std::chrono::system_clock::time_point timestamp;
};

class CachedPortfolioManager {
private:
    ServiceHost host_;
    ThreadPool thread_pool_;
    CacheManager cache_manager_;
    
    // Different cache instances for different data types
    std::shared_ptr<LRUCache<std::string, PortfolioData>> portfolio_cache_;
    std::shared_ptr<LRUCache<std::string, MarketData>> market_data_cache_;
    std::shared_ptr<LRUCache<std::string, double>> calculation_cache_;
    std::shared_ptr<LRUCache<std::string, std::string>> session_cache_;
    
public:
    CachedPortfolioManager(const std::string& service_name) 
        : host_(service_name),
          thread_pool_(4),
          cache_manager_(&host_, &thread_pool_) {
        
        // Enable distributed caching
        cache_manager_.enable_distributed_mode();
        
        // Create specialized caches for different data types
        setup_caches();
        
        // Set up message handlers
        setup_message_handlers();
        
        // Enable tracing for cache operations
        host_.enable_tracing(true);
    }
    
private:
    void setup_caches() {
        // Portfolio cache: 1000 portfolios, 1 hour TTL
        portfolio_cache_ = cache_manager_.create_distributed_cache<std::string, PortfolioData>(
            "portfolios", 1000, std::chrono::hours(1));
        
        // Market data cache: 10000 symbols, 5 minute TTL  
        market_data_cache_ = cache_manager_.create_distributed_cache<std::string, MarketData>(
            "market_data", 10000, std::chrono::minutes(5));
        
        // Calculation cache: 5000 calculations, 30 minute TTL
        calculation_cache_ = cache_manager_.create_distributed_cache<std::string, double>(
            "calculations", 5000, std::chrono::minutes(30));
        
        // Session cache: 1000 sessions, 2 hour TTL
        session_cache_ = cache_manager_.create_cache<std::string, std::string>(
            "sessions", 1000, std::chrono::hours(2));
    }
    
    void setup_message_handlers() {
        // Portfolio requests with caching
        host_.subscribe("portfolio.get", [this](const std::string& portfolio_id) {
            handle_portfolio_request(portfolio_id);
        });
        
        // Market data requests with caching
        host_.subscribe("market.get", [this](const std::string& symbol) {
            handle_market_data_request(symbol);
        });
        
        // Calculation requests with caching
        host_.subscribe("calculate.risk", [this](const std::string& request) {
            handle_risk_calculation(request);
        });
        
        // Cache management endpoints
        host_.subscribe("cache.stats", [this](const std::string&) {
            auto stats = cache_manager_.get_all_statistics();
            host_.publish_broadcast("cache.stats.response", stats);
        });
        
        host_.subscribe("cache.cleanup", [this](const std::string&) {
            cache_manager_.cleanup_all_caches();
            host_.publish_broadcast("cache.cleanup.response", "Cache cleanup completed");
        });
        
        // Cache invalidation
        host_.subscribe("cache.invalidate.portfolio", [this](const std::string& portfolio_id) {
            portfolio_cache_->remove(portfolio_id);
            // Also invalidate related calculations
            invalidate_portfolio_calculations(portfolio_id);
        });
    }
    
    void handle_portfolio_request(const std::string& portfolio_id) {
        // Try to get from cache first
        auto cached_portfolio = portfolio_cache_->get(portfolio_id);
        
        if (cached_portfolio.has_value()) {
            // Cache hit - return immediately
            host_.publish_broadcast("portfolio.response", 
                serialize_portfolio_data(cached_portfolio.value()));
            return;
        }
        
        // Cache miss - compute asynchronously
        thread_pool_.submit([this, portfolio_id]() {
            try {
                // Simulate expensive database/computation operation
                auto portfolio_data = load_portfolio_from_database(portfolio_id);
                
                // Cache the result
                portfolio_cache_->put(portfolio_id, portfolio_data);
                
                // Send response
                host_.publish_broadcast("portfolio.response", 
                    serialize_portfolio_data(portfolio_data));
                    
            } catch (const std::exception& e) {
                host_.publish_broadcast("portfolio.error", 
                    "Failed to load portfolio: " + std::string(e.what()));
            }
        });
    }
    
    void handle_market_data_request(const std::string& symbol) {
        auto cached_data = market_data_cache_->get(symbol);
        
        if (cached_data.has_value()) {
            // Check if data is still fresh (additional freshness check)
            auto now = std::chrono::system_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::minutes>(
                now - cached_data.value().timestamp);
            
            if (age < std::chrono::minutes(2)) {
                host_.publish_broadcast("market.response", 
                    serialize_market_data(cached_data.value()));
                return;
            }
        }
        
        // Fetch fresh data asynchronously
        thread_pool_.submit([this, symbol]() {
            try {
                auto market_data = fetch_market_data(symbol);
                market_data_cache_->put(symbol, market_data);
                
                host_.publish_broadcast("market.response", 
                    serialize_market_data(market_data));
                    
            } catch (const std::exception& e) {
                host_.publish_broadcast("market.error", 
                    "Failed to fetch market data: " + std::string(e.what()));
            }
        });
    }
    
    void handle_risk_calculation(const std::string& request) {
        // Create cache key from request parameters
        std::string cache_key = generate_calculation_cache_key(request);
        
        auto cached_result = calculation_cache_->get(cache_key);
        if (cached_result.has_value()) {
            host_.publish_broadcast("calculate.response", 
                std::to_string(cached_result.value()));
            return;
        }
        
        // Perform expensive calculation asynchronously
        thread_pool_.submit([this, request, cache_key]() {
            try {
                double result = perform_risk_calculation(request);
                
                // Cache result with longer TTL for expensive calculations
                calculation_cache_->put(cache_key, result, std::chrono::hours(1));
                
                host_.publish_broadcast("calculate.response", std::to_string(result));
                
            } catch (const std::exception& e) {
                host_.publish_broadcast("calculate.error", 
                    "Calculation failed: " + std::string(e.what()));
            }
        });
    }
    
    void invalidate_portfolio_calculations(const std::string& portfolio_id) {
        // Find and remove all calculation cache entries related to this portfolio
        auto keys = calculation_cache_->get_keys();
        for (const auto& key : keys) {
            if (key.find(portfolio_id) != std::string::npos) {
                calculation_cache_->remove(key);
            }
        }
    }
    
    // Simulation methods (replace with real implementations)
    PortfolioData load_portfolio_from_database(const std::string& portfolio_id) {
        // Simulate database access delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        return PortfolioData{
            .portfolio_id = portfolio_id,
            .holdings = {"AAPL", "GOOGL", "MSFT", "AMZN"},
            .total_value = 1000000.0 + (std::hash<std::string>{}(portfolio_id) % 500000),
            .last_updated = std::chrono::system_clock::now()
        };
    }
    
    MarketData fetch_market_data(const std::string& symbol) {
        // Simulate market data API call
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        return MarketData{
            .symbol = symbol,
            .price = 100.0 + (std::hash<std::string>{}(symbol) % 200),
            .volume = 1000000.0,
            .timestamp = std::chrono::system_clock::now()
        };
    }
    
    double perform_risk_calculation(const std::string& request) {
        // Simulate expensive calculation
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        return std::hash<std::string>{}(request) % 100 / 100.0;
    }
    
    std::string serialize_portfolio_data(const PortfolioData& data) {
        std::ostringstream oss;
        oss << data;
        return oss.str();
    }
    
    std::string serialize_market_data(const MarketData& data) {
        return data.symbol + ":" + std::to_string(data.price) + ":" + std::to_string(data.volume);
    }
    
    std::string generate_calculation_cache_key(const std::string& request) {
        return "calc:" + std::to_string(std::hash<std::string>{}(request));
    }

public:
    // Public interface for cache monitoring
    void print_cache_statistics() {
        std::cout << cache_manager_.get_all_statistics() << std::endl;
    }
    
    void cleanup_caches() {
        cache_manager_.cleanup_all_caches();
    }
    
    // Get individual cache statistics
    auto get_portfolio_cache_stats() { return portfolio_cache_->get_statistics(); }
    auto get_market_cache_stats() { return market_data_cache_->get_statistics(); }
    auto get_calculation_cache_stats() { return calculation_cache_->get_statistics(); }
};
