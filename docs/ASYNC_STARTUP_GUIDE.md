# ServiceHost Async Startup Guide

## Overview

The ServiceHost now supports asynchronous startup patterns that allow you to initialize service infrastructure in parallel with business logic initialization. This significantly reduces startup time for services that have expensive initialization operations.

## Key Benefits

1. **Parallel Initialization**: Infrastructure (NATS, JetStream, Cache) initializes in background while business logic loads
2. **Reduced Startup Time**: Overall service startup time is the maximum of infrastructure or business logic initialization, not the sum
3. **Better Resource Utilization**: CPU cores are utilized more efficiently during startup
4. **Improved User Experience**: Services become available faster

## Available Async Methods

### 1. `StartServiceAsync(config)`
- **Purpose**: Complete async service startup
- **Returns**: `std::future<void>` that completes when service is fully ready
- **Use Case**: When you want the entire service startup to be non-blocking

### 2. `StartServiceInfrastructureAsync(config)`
- **Purpose**: Initialize only the infrastructure (NATS, JetStream, Cache, Signal handlers)
- **Returns**: `std::future<void>` that completes when infrastructure is ready
- **Use Case**: When you want to do business logic initialization in parallel

### 3. `CompleteServiceStartup(config)`
- **Purpose**: Complete service startup after infrastructure is ready
- **Requirements**: Must be called after infrastructure future is complete
- **Use Case**: Finish service initialization after parallel operations

## Usage Patterns

### Pattern 1: Full Async Startup

```cpp
class MyService {
    std::unique_ptr<ServiceHost> service_host_;
    
public:
    MyService(const std::string& uid) 
        : service_host_(std::make_unique<ServiceHost>(uid, "MyService")) {}
    
    void start_async() {
        auto config = ServiceHost::create_production_config();
        
        // Setup callbacks
        config.metrics_flush_callback = [this]() { flush_metrics(); };
        config.health_heartbeat_callback = [this]() { send_health(); };
        
        // Start service asynchronously
        auto future = service_host_->StartServiceAsync(config);
        
        // Continue with other work...
        initialize_business_logic();
        
        // Wait for service to be ready
        future.get();
        
        std::cout << "Service ready!" << std::endl;
    }
};
```

### Pattern 2: Parallel Infrastructure and Business Logic

```cpp
class MyService {
    std::unique_ptr<ServiceHost> service_host_;
    
public:
    void start_with_parallel_init() {
        auto config = ServiceHost::create_production_config();
        
        // 1. Start infrastructure in background
        auto infra_future = service_host_->StartServiceInfrastructureAsync(config);
        
        // 2. Do expensive business logic initialization in parallel
        load_database_data();      // 500ms
        initialize_algorithms();   // 300ms
        setup_internal_services(); // 200ms
        
        // 3. Wait for infrastructure and complete startup
        infra_future.get();
        setup_message_handlers();  // Requires infrastructure to be ready
        service_host_->CompleteServiceStartup(config);
        
        std::cout << "Service ready with parallel initialization!" << std::endl;
    }
};
```

### Pattern 3: Portfolio Manager Example

```cpp
class PortfolioManagerAsync {
    std::unique_ptr<ServiceHost> service_host_;
    
public:
    void start_with_parallel_init() {
        std::cout << "🚀 Starting with parallel initialization..." << std::endl;
        
        // Start infrastructure in background
        auto infra_future = start_infrastructure_async();
        
        // Parallel business logic initialization
        std::cout << "📊 Loading portfolio data..." << std::endl;
        load_portfolio_data();        // 500ms - load from database
        
        std::cout << "💼 Initializing business logic..." << std::endl;
        initialize_business_logic();  // 300ms - setup algorithms
        
        std::cout << "🔧 Setting up internal services..." << std::endl;
        setup_internal_services();   // 200ms - configure components
        
        // Wait for infrastructure and complete
        std::cout << "⏳ Waiting for infrastructure..." << std::endl;
        complete_startup(std::move(infra_future));
        
        std::cout << "✅ Fully initialized with parallel startup!" << std::endl;
    }
    
private:
    std::future<void> start_infrastructure_async() {
        auto config = ServiceHost::create_production_config();
        config.enable_cache = true;
        config.default_cache_size = 5000;
        config.default_cache_ttl = std::chrono::hours(2);
        
        // Setup callbacks
        config.metrics_flush_callback = [this]() { flush_portfolio_metrics(); };
        config.health_heartbeat_callback = [this]() { send_portfolio_health(); };
        config.backpressure_callback = [this]() { handle_backpressure(); };
        
        return service_host_->StartServiceInfrastructureAsync(config);
    }
    
    void complete_startup(std::future<void>&& infrastructure_future) {
        infrastructure_future.get();
        setup_message_handlers();
        
        auto config = ServiceHost::create_production_config();
        // ... configure callbacks ...
        
        service_host_->CompleteServiceStartup(config);
    }
};
```

## Performance Impact

### Before (Sequential Startup)
```
Infrastructure Init: 200ms
Business Logic Init: 500ms
Total Startup Time: 700ms
```

### After (Parallel Startup)
```
Infrastructure Init: 200ms (in background)
Business Logic Init: 500ms (in parallel)
Total Startup Time: 500ms (max of both)
```

**Result**: ~30% faster startup time in this example.

## Best Practices

### 1. Identify Parallel Operations
- Infrastructure initialization (NATS, JetStream, Cache)
- Database loading
- Algorithm initialization
- Configuration parsing
- External service connections

### 2. Handle Dependencies Correctly
- Message handlers require infrastructure to be ready
- Some business logic may depend on cache being available
- Always wait for infrastructure future before setting up handlers

### 3. Error Handling
```cpp
void start_with_error_handling() {
    try {
        auto infra_future = start_infrastructure_async();
        
        // Do parallel work
        initialize_business_logic();
        
        // Wait for infrastructure
        infra_future.get();  // This will throw if infrastructure failed
        
        complete_startup();
    } catch (const std::exception& e) {
        std::cerr << "Startup failed: " << e.what() << std::endl;
        throw;
    }
}
```

### 4. Timeouts
```cpp
void start_with_timeout() {
    auto infra_future = start_infrastructure_async();
    
    // Do parallel work
    initialize_business_logic();
    
    // Wait with timeout
    if (infra_future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        infra_future.get();
        complete_startup();
    } else {
        throw std::runtime_error("Infrastructure initialization timeout");
    }
}
```

## Configuration

All async methods use the same `ServiceInitConfig` structure:

```cpp
ServiceInitConfig config = ServiceHost::create_production_config();

// Customize for your service
config.enable_cache = true;
config.default_cache_size = 5000;
config.default_cache_ttl = std::chrono::hours(2);

// Setup callbacks
config.metrics_flush_callback = [this]() { /* metrics logic */ };
config.health_heartbeat_callback = [this]() { /* health logic */ };
config.backpressure_callback = [this]() { /* backpressure logic */ };
config.queue_size_func = [this]() { return get_queue_size(); };
```

## Thread Safety

- All async methods are thread-safe
- Futures can be moved between threads
- ServiceHost handles internal synchronization
- Business logic initialization should be thread-safe if called from multiple threads

## Monitoring and Logging

The async startup provides comprehensive logging:

```
🚀 Starting async infrastructure initialization for: MyService
📡 Initializing NATS connection: nats://localhost:4222
✅ Connected to NATS: nats://localhost:4222
🚀 Initializing JetStream
✅ JetStream initialized successfully
🧠 Initializing cache system (default: 5000 items, TTL: 7200s)
✅ Infrastructure initialization completed successfully
```

This allows you to monitor the progress of both infrastructure and business logic initialization in parallel.

## Migration from Synchronous

### Before:
```cpp
void start() {
    setup_handlers();
    service_host_->StartService(config);
}
```

### After:
```cpp
void start_parallel() {
    auto infra_future = service_host_->StartServiceInfrastructureAsync(config);
    
    // Do parallel initialization
    initialize_business_logic();
    
    // Complete startup
    infra_future.get();
    setup_handlers();
    service_host_->CompleteServiceStartup(config);
}
```

The async pattern provides significant performance benefits for services with substantial initialization requirements while maintaining the same reliability and feature set.
