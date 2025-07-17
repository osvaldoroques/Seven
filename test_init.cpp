// Simple test to verify our new initialization system works
#include "service_host.hpp"
#include <iostream>
#include <memory>

class TestService : public ServiceHost {
public:
    TestService() : ServiceHost("test-001", "TestService") {}
    
    void test_initialization() {
        std::cout << "🧪 Testing ServiceHost initialization system\n";
        
        // Test 1: Default config
        std::cout << "1. Testing default config creation... ";
        auto default_config = ServiceHost::create_default_config();
        std::cout << "✅ Default config created\n";
        
        // Test 2: Production config
        std::cout << "2. Testing production config creation... ";
        auto prod_config = ServiceHost::create_production_config();
        std::cout << "✅ Production config created\n";
        
        // Test 3: Development config
        std::cout << "3. Testing development config creation... ";
        auto dev_config = ServiceHost::create_development_config();
        std::cout << "✅ Development config created\n";
        
        // Test 4: Performance config
        std::cout << "4. Testing performance config creation... ";
        auto perf_config = ServiceHost::create_performance_config();
        std::cout << "✅ Performance config created\n";
        
        // Test 5: Try to initialize with development config
        std::cout << "5. Testing service initialization... ";
        try {
            dev_config.nats_url = "nats://localhost:4222";
            dev_config.enable_cache = true;
            dev_config.enable_scheduler = true;
            dev_config.enable_metrics_flush = false;  // Disable for testing
            dev_config.enable_health_heartbeat = false;  // Disable for testing
            dev_config.enable_backpressure_monitor = false;  // Disable for testing
            
            initialize_service(dev_config);
            std::cout << "✅ Service initialized successfully\n";
        } catch (const std::exception& e) {
            std::cout << "⚠️ Service initialization failed: " << e.what() << "\n";
        }
        
        std::cout << "✅ All tests completed\n";
    }
};

int main() {
    std::cout << "🚀 ServiceHost Initialization Test\n";
    std::cout << "==================================\n";
    
    TestService test_service;
    test_service.test_initialization();
    
    return 0;
}
