# ServiceHost Unified Initialization System

## Overview

The ServiceHost framework now includes a comprehensive unified initialization system that consolidates all service configuration into a single, easy-to-use interface. This eliminates the need for repetitive setup code and ensures consistent service initialization across different environments.

## Key Features

### 1. ServiceInitConfig Structure
A comprehensive configuration structure that includes:
- **NATS Configuration**: Connection URL, JetStream settings
- **Cache Configuration**: Enable/disable caching, default size and TTL
- **Scheduler Configuration**: Task scheduling, auto-cleanup settings
- **Metrics Configuration**: Flush intervals, custom callback functions
- **Health Monitoring**: Heartbeat intervals, health check callbacks
- **Backpressure Monitoring**: Queue size monitoring, threshold settings
- **Performance Mode**: Optimization settings for different environments

### 2. Pre-configured Environment Settings
Four factory methods for common deployment scenarios:

#### `create_default_config()`
- Basic configuration suitable for development and testing
- Moderate cache size (5000 items)
- Standard monitoring intervals
- Full observability enabled

#### `create_production_config()`
- Optimized for production environments
- Large cache size (20000 items)
- Aggressive monitoring and metrics collection
- Performance optimizations enabled

#### `create_development_config()`
- Development-friendly settings
- Smaller cache size (1000 items)
- Verbose logging and tracing
- Lower backpressure thresholds for testing

#### `create_performance_config()`
- High-performance scenarios
- Very large cache (50000 items)
- Minimal overhead monitoring
- Optimized for throughput

### 3. Unified Initialization Method
The `initialize_service(const ServiceInitConfig& config)` method handles:
- NATS connection setup with JetStream
- Cache system initialization
- Task scheduler configuration
- Metrics collection setup
- Health monitoring activation
- Backpressure monitoring
- Error handling and validation

## Usage Examples

### Basic Usage
```cpp
// Create a service
MyService service("service-001", "MyService");

// Initialize with default settings
auto config = ServiceHost::create_default_config();
config.nats_url = "nats://localhost:4222";
service.initialize_service(config);
```

### Production Environment
```cpp
// Production initialization
auto config = ServiceHost::create_production_config();
config.nats_url = "nats://nats:4222";

// Add production-specific callbacks
config.metrics_flush_callback = [&service]() {
    service.flush_business_metrics();
};

config.health_heartbeat_callback = [&service]() {
    service.send_health_status();
};

config.queue_size_func = [&service]() -> size_t {
    return service.get_thread_pool().pending_tasks();
};

config.backpressure_callback = [&service]() {
    service.handle_high_load();
};

service.initialize_service(config);
```

### Custom Configuration
```cpp
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

// Metrics settings
config.enable_metrics_flush = true;
config.metrics_flush_interval = std::chrono::seconds(60);
config.metrics_flush_callback = []() {
    // Custom metrics collection
};

// Health monitoring
config.enable_health_heartbeat = true;
config.health_heartbeat_interval = std::chrono::seconds(15);
config.health_heartbeat_callback = []() {
    // Custom health check
};

service.initialize_service(config);
```

## Service-Specific Integration

The PortfolioManager class now includes service-specific initialization methods:

### `initialize_portfolio_service(const ServiceInitConfig& config)`
- Initializes the service with portfolio-specific settings
- Configures business logic callbacks
- Sets up portfolio-specific monitoring

### `initialize_for_development()`
- Development-optimized settings
- Lower resource usage
- Enhanced debugging capabilities

### `initialize_for_high_performance()`
- High-throughput optimization
- Minimal overhead
- Large cache sizes

## Environment-Based Initialization

The main.cpp now supports environment-based initialization:

```cpp
int main(int argc, char* argv[]) {
    PortfolioManager service("portfolio-001", "PortfolioManager");
    
    // Environment-based initialization
    const char* env = std::getenv("DEPLOYMENT_ENV");
    if (env && std::string(env) == "production") {
        service.initialize_for_production();
    } else if (env && std::string(env) == "performance") {
        service.initialize_for_high_performance();
    } else {
        service.initialize_for_development();
    }
    
    // Service is now ready to use
    return 0;
}
```

## Benefits

1. **Consistency**: All services use the same initialization pattern
2. **Flexibility**: Easy to customize for different environments
3. **Maintainability**: Single source of truth for service configuration
4. **Performance**: Pre-optimized configurations for different scenarios
5. **Developer Experience**: Simple, intuitive API
6. **Error Handling**: Comprehensive validation and error reporting

## Migration Guide

### From Manual Setup
```cpp
// Old way (manual setup)
MyService service("service-001", "MyService");
service.connect_to_nats("nats://localhost:4222");
service.enable_cache(5000, std::chrono::hours(1));
service.enable_scheduler();
service.setup_metrics_flush(std::chrono::minutes(5));
service.setup_health_monitoring(std::chrono::seconds(30));

// New way (unified initialization)
auto config = ServiceHost::create_default_config();
config.nats_url = "nats://localhost:4222";
service.initialize_service(config);
```

### From Environment-Specific Code
```cpp
// Old way (environment-specific code)
#ifdef PRODUCTION
    service.setup_production_config();
#else
    service.setup_development_config();
#endif

// New way (environment-based factory)
auto config = ServiceHost::create_production_config();
service.initialize_service(config);
```

This unified initialization system significantly improves the developer experience while ensuring consistent, robust service setup across all environments.
