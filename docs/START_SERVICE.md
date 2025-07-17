# ServiceHost StartService Method

## Overview

The `StartService` method provides a comprehensive, one-stop solution for service initialization and startup. It handles all aspects of service configuration and startup based on the `ServiceInitConfig` passed to it.

## Method Signature

```cpp
void StartService(const ServiceInitConfig& config = {});
```

## Features

The `StartService` method automatically handles:

1. **Service Initialization** - Calls `initialize_service()` with the provided configuration
2. **Signal Handling** - Sets up graceful shutdown handlers for SIGINT and SIGTERM
3. **Subscription Processing** - Activates all registered message handlers
4. **Runtime State** - Sets the service as running and ready to process messages
5. **Comprehensive Logging** - Provides detailed startup summary with all configuration details

## Configuration Options

The method accepts a `ServiceInitConfig` struct with the following options:

### NATS Configuration
- `nats_url`: NATS server URL (default: "nats://localhost:4222")
- `enable_jetstream`: Enable JetStream support (default: true)

### Cache Configuration
- `enable_cache`: Enable caching system (default: true)
- `default_cache_size`: Default cache size (default: 1000)
- `default_cache_ttl`: Default cache TTL (default: 1 hour)

### Scheduler Configuration
- `enable_scheduler`: Enable task scheduler (default: true)
- `enable_auto_cache_cleanup`: Enable automatic cache cleanup (default: true)
- `cache_cleanup_interval`: Cache cleanup interval (default: 5 minutes)

### Monitoring & Metrics
- `enable_metrics_flush`: Enable metrics flushing (default: false)
- `metrics_flush_interval`: Metrics flush interval (default: 30 seconds)
- `metrics_flush_callback`: Callback function for metrics flushing

### Health Check Configuration
- `enable_health_heartbeat`: Enable health heartbeat (default: false)
- `health_heartbeat_interval`: Health heartbeat interval (default: 10 seconds)
- `health_heartbeat_callback`: Callback function for health heartbeat

### Back-pressure Monitoring
- `enable_backpressure_monitor`: Enable backpressure monitoring (default: false)
- `backpressure_threshold`: Backpressure threshold (default: 100)
- `queue_size_func`: Function to get current queue size
- `backpressure_callback`: Callback function for backpressure alerts

### Performance Configuration
- `enable_performance_mode`: Enable performance mode (default: false)
- `force_otel_initialization`: Force OpenTelemetry initialization (default: false)
- `custom_otel_endpoint`: Custom OpenTelemetry endpoint

## Usage Examples

### Basic Usage
```cpp
class MyService {
    std::unique_ptr<ServiceHost> service_host_;
    
public:
    MyService(const std::string& uid)
        : service_host_(std::make_unique<ServiceHost>(uid, "MyService"))
    {
        // Register message handlers
        service_host_->register_message<MyMessage>(
            MessageRouting::PointToPoint,
            [this](const MyMessage& msg) {
                // Handle message
            }
        );
    }
    
    void start() {
        // Start with default configuration
        service_host_->StartService();
    }
};
```

### Production Configuration
```cpp
void start() {
    auto config = ServiceHost::create_production_config();
    
    // Add custom callbacks
    config.metrics_flush_callback = [this]() {
        flush_metrics();
    };
    
    config.health_heartbeat_callback = [this]() {
        send_health_status();
    };
    
    config.backpressure_callback = [this]() {
        handle_backpressure();
    };
    
    service_host_->StartService(config);
}
```

### Custom Configuration
```cpp
void start() {
    ServiceInitConfig config;
    config.enable_cache = true;
    config.default_cache_size = 10000;
    config.default_cache_ttl = std::chrono::hours(4);
    config.enable_metrics_flush = true;
    config.enable_health_heartbeat = true;
    config.enable_backpressure_monitor = true;
    config.backpressure_threshold = 200;
    
    // Set callbacks
    config.metrics_flush_callback = [this]() {
        // Custom metrics logic
    };
    
    service_host_->StartService(config);
}
```

### High-Performance Configuration
```cpp
void start() {
    auto config = ServiceHost::create_performance_config();
    
    // High-performance settings for low-latency services
    config.enable_performance_mode = true;  // Disables tracing
    config.enable_metrics_flush = false;    // Reduces overhead
    config.enable_health_heartbeat = false; // Minimal monitoring
    
    service_host_->StartService(config);
}
```

## Benefits

1. **Simplified Setup**: Single method call handles all initialization
2. **Consistent Configuration**: Standardized configuration across all services
3. **Comprehensive Logging**: Detailed startup information and diagnostics
4. **Flexible Configuration**: Pre-built configurations for common scenarios
5. **Production Ready**: Built-in monitoring, metrics, and health checks
6. **Clean Architecture**: Separates configuration from business logic

## Integration with PortfolioManager

The PortfolioManager now uses the `StartService` method:

```cpp
void start() {
    auto config = ServiceHost::create_production_config();
    
    // Portfolio-specific configuration
    config.enable_cache = true;
    config.default_cache_size = 5000;
    config.default_cache_ttl = std::chrono::hours(2);
    
    // Setup callbacks
    config.metrics_flush_callback = [this]() {
        _flush_portfolio_metrics();
    };
    
    config.health_heartbeat_callback = [this]() {
        _send_portfolio_health_status();
    };
    
    config.backpressure_callback = [this]() {
        _handle_portfolio_backpressure();
    };
    
    service_host_->StartService(config);
}
```

This provides a clean, maintainable, and production-ready service startup process.
