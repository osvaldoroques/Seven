// Example: Using ServiceHost initialize_service() function
// This shows how to use the new comprehensive service initialization

#include "service_host.hpp"
#include <iostream>

class MyMicroservice : public ServiceHost {
public:
    MyMicroservice(const std::string& uid, const std::string& service_name) 
        : ServiceHost(uid, service_name) {
        std::cout << "✅ " << service_name << " created with UID: " << uid << std::endl;
    }
    
    // Example: Initialize with default settings
    void initialize_with_defaults() {
        std::cout << "\n🚀 Initializing with default settings...\n";
        
        // Use default configuration
        auto config = ServiceHost::create_default_config();
        config.nats_url = "nats://localhost:4222";
        
        initialize_service(config);
    }
    
    // Example: Initialize for production
    void initialize_for_production() {
        std::cout << "\n🏭 Initializing for production...\n";
        
        auto config = ServiceHost::create_production_config();
        config.nats_url = "nats://nats:4222";  // Docker service name
        
        // Add custom callbacks for production
        config.metrics_flush_callback = [this]() {
            flush_business_metrics();
        };
        
        config.health_heartbeat_callback = [this]() {
            send_health_status();
        };
        
        config.queue_size_func = [this]() -> size_t {
            return get_thread_pool().pending_tasks();
        };
        
        config.backpressure_callback = [this]() {
            handle_high_load();
        };
        
        initialize_service(config);
    }
    
    // Example: Initialize for development
    void initialize_for_development() {
        std::cout << "\n🛠️ Initializing for development...\n";
        
        auto config = ServiceHost::create_development_config();
        config.nats_url = "nats://localhost:4222";
        
        // Development-specific settings
        config.enable_performance_mode = false;  // Full tracing for debugging
        config.backpressure_threshold = 20;      // Lower threshold for testing
        
        initialize_service(config);
    }
    
    // Example: Initialize for high-performance scenarios
    void initialize_for_high_performance() {
        std::cout << "\n⚡ Initializing for high-performance...\n";
        
        auto config = ServiceHost::create_performance_config();
        config.nats_url = "nats://localhost:4222";
        
        // High-performance specific settings
        config.default_cache_size = 50000;       // Very large cache
        config.default_cache_ttl = std::chrono::minutes(10);  // Short TTL
        
        initialize_service(config);
    }
    
    // Example: Custom initialization with specific requirements
    void initialize_custom() {
        std::cout << "\n🎯 Initializing with custom configuration...\n";
        
        ServiceInitConfig config;
        
        // NATS settings
        config.nats_url = "nats://localhost:4222";
        config.enable_jetstream = true;
        
        // Cache settings
        config.enable_cache = true;
        config.default_cache_size = 10000;
        config.default_cache_ttl = std::chrono::hours(4);
        
        // Scheduler settings
        config.enable_scheduler = true;
        config.enable_auto_cache_cleanup = true;
        config.cache_cleanup_interval = std::chrono::minutes(10);
        
        // Metrics (custom implementation)
        config.enable_metrics_flush = true;
        config.metrics_flush_interval = std::chrono::seconds(60);
        config.metrics_flush_callback = [this]() {
            // Custom metrics collection
            auto cache_stats = get_cache().get_all_cache_stats();
            auto scheduler_stats = get_scheduler().get_scheduler_stats();
            
            get_logger()->info("📊 Metrics - Cache instances: {}, Scheduler tasks: {}", 
                             cache_stats.size(), scheduler_stats.active_tasks);
        };
        
        // Health monitoring
        config.enable_health_heartbeat = true;
        config.health_heartbeat_interval = std::chrono::seconds(15);
        config.health_heartbeat_callback = [this]() {
            get_logger()->info("❤️ Service healthy: {}", get_status());
        };
        
        // Backpressure monitoring
        config.enable_backpressure_monitor = true;
        config.backpressure_threshold = 75;
        config.queue_size_func = [this]() -> size_t {
            return get_thread_pool().pending_tasks();
        };
        config.backpressure_callback = [this]() {
            get_logger()->warn("⚠️ High queue size detected!");
            // Custom backpressure handling
        };
        
        // Performance mode
        config.enable_performance_mode = false;  // Full observability
        
        initialize_service(config);
    }
    
private:
    void flush_business_metrics() {
        get_logger()->debug("📊 Flushing business metrics");
        // Implementation: collect and send metrics
    }
    
    void send_health_status() {
        get_logger()->debug("❤️ Sending health status");
        // Implementation: send health check
    }
    
    void handle_high_load() {
        get_logger()->warn("⚠️ Handling high load situation");
        // Implementation: handle backpressure
    }
};

int main() {
    std::cout << "🚀 ServiceHost Initialization Examples\n";
    std::cout << "======================================\n";
    
    MyMicroservice service("service-001", "ExampleService");
    
    // Example 1: Default initialization
    // service.initialize_with_defaults();
    
    // Example 2: Production initialization
    // service.initialize_for_production();
    
    // Example 3: Development initialization
    // service.initialize_for_development();
    
    // Example 4: High-performance initialization
    // service.initialize_for_high_performance();
    
    // Example 5: Custom initialization
    service.initialize_custom();
    
    std::cout << "\n✅ Service initialization complete!\n";
    std::cout << "🎯 Service Status: " << service.get_status() << std::endl;
    std::cout << "🧵 Worker Threads: " << service.get_thread_pool().size() << std::endl;
    
    // Keep the service running
    std::cout << "\n🔄 Service running... Press Ctrl+C to stop\n";
    
    // Simple message loop
    while (service.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n🛑 Service stopped\n";
    return 0;
}
