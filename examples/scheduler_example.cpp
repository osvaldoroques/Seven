// Example usage of ServiceScheduler in a service

#include "service_host.hpp"
#include <iostream>

class MyService {
public:
    MyService() : host_("service-1", "my-service") {
        // Initialize the service
        setup_scheduled_tasks();
    }
    
    void run() {
        // Your service logic here
        std::cout << "Service running..." << std::endl;
        
        // Sleep to see scheduled tasks run
        std::this_thread::sleep_for(std::chrono::minutes(2));
    }
    
private:
    ServiceHost host_;
    
    void setup_scheduled_tasks() {
        // 1. Schedule metrics flush every 30 seconds
        host_.schedule_metrics_flush([this]() {
            flush_metrics();
        });
        
        // 2. Schedule cache cleanup every 5 minutes
        host_.schedule_cache_cleanup([this]() {
            cleanup_cache();
        });
        
        // 3. Schedule health heartbeat every 10 seconds
        host_.schedule_health_heartbeat([this]() {
            send_health_heartbeat();
        });
        
        // 4. Schedule back-pressure monitoring
        host_.schedule_backpressure_monitor(
            [this]() -> size_t { return get_queue_size(); },  // Queue size function
            100,                                              // Threshold
            [this]() { handle_backpressure(); }              // Alert function
        );
        
        // 5. Custom scheduled task - cleanup old data every hour
        host_.schedule_interval("cleanup_old_data", std::chrono::hours(1), [this]() {
            cleanup_old_data();
        });
        
        // 6. One-time task - send startup notification after 30 seconds
        host_.schedule_once("startup_notification", std::chrono::seconds(30), [this]() {
            send_startup_notification();
        });
        
        std::cout << "Scheduled tasks configured" << std::endl;
    }
    
    void flush_metrics() {
        std::cout << "📊 Flushing metrics..." << std::endl;
        // Collect and send metrics to monitoring system
    }
    
    void cleanup_cache() {
        std::cout << "🧹 Cleaning up cache..." << std::endl;
        // Clean expired entries from cache
        host_.get_cache().cleanup_expired();
    }
    
    void send_health_heartbeat() {
        std::cout << "❤️ Sending health heartbeat..." << std::endl;
        // Send health status to monitoring system
    }
    
    size_t get_queue_size() {
        // Return current message queue size
        return host_.get_thread_pool().pending_tasks();
    }
    
    void handle_backpressure() {
        std::cout << "⚠️ High queue size detected! Handling backpressure..." << std::endl;
        // Implement backpressure handling (e.g., reject new requests, scale up)
    }
    
    void cleanup_old_data() {
        std::cout << "🗑️ Cleaning up old data..." << std::endl;
        // Remove old data files, logs, etc.
    }
    
    void send_startup_notification() {
        std::cout << "🚀 Service startup complete!" << std::endl;
        // Send notification that service is fully initialized
    }
};

int main() {
    MyService service;
    service.run();
    return 0;
}
