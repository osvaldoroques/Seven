#pragma once

#include "service_host.hpp"
#include "messages.pb.h"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <future>
#include <thread>

class PortfolioManager {
private:
    std::unique_ptr<ServiceHost> service_host_;
    
public:    
    // Constructor with config file
    PortfolioManager(const std::string& uid, const std::string& config_file)
        : service_host_(std::make_unique<ServiceHost>(uid, "PortfolioManager", config_file))
    {
        std::cout << "✅ PortfolioManager initialized with UID: " << uid << " and config: " << config_file << std::endl;
    }
    
    // Destructor
    ~PortfolioManager() = default;
    
    // 🚀 NEW: Start infrastructure asynchronously
    std::future<void> start_infrastructure_async() {
        // Create configuration for portfolio service
        auto config = ServiceHost::create_production_config();
        
        // Portfolio-specific configuration
        config.enable_cache = true;
        config.default_cache_size = 5000;  // Large cache for portfolio data
        config.default_cache_ttl = std::chrono::hours(2);
        
        // Note: Metrics flush, health status, and backpressure monitoring 
        // are now handled automatically by ServiceHost permanent tasks
        // No need for custom callbacks - ServiceHost handles it all!
        
        // Start infrastructure in background and return the future directly
        std::cout << "🚀 PortfolioManager infrastructure starting in background..." << std::endl;
        return service_host_->StartServiceInfrastructureAsync(config);
    }
    
    // 🚀 NEW: Complete service startup after infrastructure is ready
    void complete_startup(std::future<void>&& infrastructure_future) {
        // Wait for infrastructure to be ready
        infrastructure_future.get();
        
        // Setup message handlers now that infrastructure is ready
        _setup_handlers();
        
        // Complete the service startup with simplified config
        auto config = ServiceHost::create_production_config();
        config.enable_cache = true;
        config.default_cache_size = 5000;
        config.default_cache_ttl = std::chrono::hours(2);
        
        // Note: ServiceHost permanent tasks handle all maintenance automatically
        // No need for custom callbacks - cleaner and more consistent!
        
        service_host_->CompleteServiceStartup(config);
        
        std::cout << "🚀 PortfolioManager startup completed successfully" << std::endl;
    }
    
    // 🚀 NEW: Start with parallel initialization
    void start_with_parallel_init() {
        std::cout << "🚀 Starting PortfolioManager with parallel initialization..." << std::endl;
        
        // Start infrastructure in background
        auto infra_future = start_infrastructure_async();
        
        // Meanwhile, do other initialization work in parallel
        std::cout << "📊 Loading portfolio data in parallel..." << std::endl;
        _load_portfolio_data();
        
        std::cout << "💼 Initializing business logic..." << std::endl;
        _initialize_business_logic();
        
        std::cout << "🔧 Setting up internal services..." << std::endl;
        _setup_internal_services();
        
        // Wait for infrastructure to complete and finish startup
        std::cout << "⏳ Waiting for infrastructure to complete..." << std::endl;
        complete_startup(std::move(infra_future));
        
        std::cout << "✅ PortfolioManager fully initialized with parallel startup!" << std::endl;
    }
    
    // Traditional synchronous start method
    void start() {
        auto config = ServiceHost::create_production_config();
        config.enable_cache = true;
        config.default_cache_size = 5000;
        config.default_cache_ttl = std::chrono::hours(2);
        
        // Note: ServiceHost permanent tasks handle metrics, health, and backpressure automatically
        // This eliminates the need for service-specific callback implementations
        
        // Setup handlers first
        _setup_handlers();
        
        // Start the service with all configurations
        service_host_->StartService(config);
        
        std::cout << "🚀 PortfolioManager started successfully" << std::endl;
    }
    
    // Run method: start and let ServiceHost handle the rest
    void run() {
        start();
        
        // Main service loop - ServiceHost handles shutdown
        while (service_host_->is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        service_host_->get_logger()->info("Portfolio Manager shutting down...");
        service_host_->shutdown();
    }
    
    // 🚀 NEW: Parallel run method
    void run_parallel() {
        start_with_parallel_init();
        
        // Main service loop - ServiceHost handles shutdown
        while (service_host_->is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        service_host_->get_logger()->info("Portfolio Manager shutting down...");
        service_host_->shutdown();
    }
    
private:
    // Setup handlers during construction
    void _setup_handlers() {
        // Register handlers using ServiceHost's register_message method
        service_host_->register_message<Trevor::HealthCheckRequest>(
            MessageRouting::PointToPoint,
            [this](const Trevor::HealthCheckRequest& req) {
                service_host_->get_logger()->info("📋 Received HealthCheckRequest from service: {}, UID: {}", 
                                                  req.service_name(), req.uid());
                
                // Create response
                Trevor::HealthCheckResponse res;
                res.set_service_name("PortfolioManager");
                res.set_uid(service_host_->get_uid());
                res.set_status(service_host_->get_status());
                
                // Send response
                service_host_->publish_point_to_point(req.uid(), res);
                service_host_->get_logger()->info("✅ Sent HealthCheckResponse to: {}", req.uid());
            }
        );
        
        service_host_->register_message<Trevor::PortfolioRequest>(
            MessageRouting::PointToPoint,
            [this](const Trevor::PortfolioRequest& req) {
                service_host_->get_logger()->info("💼 Processing PortfolioRequest for account: {}", req.account_id());
                
                // Business logic: Calculate portfolio value
                double portfolio_value = _calculate_portfolio_value(req.account_id());
                
                // Create response
                Trevor::PortfolioResponse res;
                res.set_account_id(req.account_id());
                res.set_total_value(portfolio_value);
                res.set_cash_balance(25000.0);  // Example cash balance
                res.set_status("active");
                
                // Send response
                service_host_->publish_point_to_point(req.requester_uid(), res);
                service_host_->get_logger()->info("✅ Sent PortfolioResponse for account: {}", req.account_id());
            }
        );
        
        service_host_->register_message<Trevor::MarketDataUpdate>(
            MessageRouting::Broadcast,
            [this](const Trevor::MarketDataUpdate& update) {
                service_host_->get_logger()->debug("📈 Market Data Update - Symbol: {}, Price: ${}, Volume: {}", 
                                                   update.symbol(), update.price(), update.volume());
                
                // Business logic: Update portfolio calculations
                _update_portfolio_calculations(update.symbol(), update.price(), update.volume());
            }
        );
    }
    
    // Parallel initialization methods
    void _load_portfolio_data() {
        // Simulate loading portfolio data from database
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "📊 Portfolio data loaded successfully" << std::endl;
    }
    
    void _initialize_business_logic() {
        // Simulate business logic initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "💼 Business logic initialized successfully" << std::endl;
    }
    
    void _setup_internal_services() {
        // Simulate internal services setup
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "🔧 Internal services setup completed" << std::endl;
    }
    
    // Business logic methods
    double _calculate_portfolio_value(const std::string& account_id) {
        double default_value = service_host_->get_config<double>("portfolio_manager.default_portfolio_value", 100000.0);
        service_host_->get_logger()->debug("Calculated portfolio value for {}: ${}", account_id, default_value);
        return default_value;
    }
    
    void _update_portfolio_calculations(const std::string& symbol, double price, int64_t volume) {
        service_host_->get_logger()->trace("Updated calculations for {} at ${}", symbol, price);
    }
};
