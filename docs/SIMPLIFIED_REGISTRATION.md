# 🚀 Simplified Service Registration System

## Overview

The Seven framework now supports a simplified handler registration system that allows developers to focus on business logic instead of complex registration macros. Services can now register handlers using a clean map-based approach with raw payload handling.

## Key Features

### 1. **Raw Payload Handlers**
- Handlers receive raw string payloads
- No complex template macros required
- Simple protobuf parsing in business logic
- Clean separation of concerns

### 2. **Map-Based Registration**
- `RegistrationMap` for batch registration
- Message type string → (routing, handler) mapping
- Easy to read and maintain
- Supports both point-to-point and broadcast routing

### 3. **Service-Focused Development**
- Business logic methods (e.g., `onHealthCheckRaw()`)
- Minimal configuration required
- Clear method names and purposes
- Easy testing and debugging

## Usage Example

### Before (Complex Registration):
```cpp
PortfolioManager svc(
    PortfolioManager::ConfigFileTag{},
    "svc-portfolio-001",
    config_file,
    MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint, 
        ([&svc](const Trevor::HealthCheckRequest& req) {
            // Complex lambda with template magic
            auto trace_context = svc.extract_trace_context_from_message(req);
            TRACE_SPAN_WITH_CONTEXT("HealthCheck::Process", trace_context);
            // ... more complex code
        })),
    MSG_REG(Trevor::PortfolioRequest, MessageRouting::PointToPoint, 
        ([&svc](const Trevor::PortfolioRequest& req) {
            // Another complex lambda
            // ... more complex code
        })),
    // ... more MSG_REG calls
);
```

### After (Simplified Registration):
```cpp
// 1. Create service with simple constructor
PortfolioManager svc(
    PortfolioManager::ConfigFileTag{},
    "svc-portfolio-001", 
    config_file
);

// 2. Register handlers using simple map
svc.register_handlers(svc.get_handler_registrations());

// 3. Initialize service
svc.initialize_portfolio_service(nats_url);
```

## Business Logic Implementation

### Service Class Structure:
```cpp
class PortfolioManager : public ServiceHost {
public:
    // Simple constructors
    PortfolioManager(ConfigFileTag, const std::string& uid, const std::string& config_file);
    
    // Business logic handlers - clean and focused
    void onHealthCheckRaw(const std::string& payload);
    void onPortfolioRequestRaw(const std::string& payload);
    void onMarketDataUpdateRaw(const std::string& payload);
    
    // Registration map - easy to read and maintain
    ServiceHost::RegistrationMap get_handler_registrations() {
        return {
            {"Trevor.HealthCheckRequest", {MessageRouting::PointToPoint, 
                [this](const std::string& payload) { onHealthCheckRaw(payload); }}},
            
            {"Trevor.PortfolioRequest", {MessageRouting::PointToPoint, 
                [this](const std::string& payload) { onPortfolioRequestRaw(payload); }}},
            
            {"Trevor.MarketDataUpdate", {MessageRouting::Broadcast, 
                [this](const std::string& payload) { onMarketDataUpdateRaw(payload); }}}
        };
    }
    
private:
    // Business logic methods
    double calculate_portfolio_value(const std::string& account_id);
    void update_portfolio_calculations(const std::string& symbol, double price, int64_t volume);
};
```

### Handler Implementation Example:
```cpp
void PortfolioManager::onHealthCheckRaw(const std::string& payload) {
    get_logger()->info("📋 Processing health check request");
    
    // Parse the protobuf message from raw payload
    Trevor::HealthCheckRequest req;
    if (!req.ParseFromString(payload)) {
        get_logger()->error("❌ Failed to parse HealthCheckRequest");
        return;
    }
    
    get_logger()->info("Received HealthCheckRequest from service: {}, UID: {}", 
                      req.service_name(), req.uid());
    
    // Create response
    Trevor::HealthCheckResponse res;
    res.set_service_name("PortfolioManager");
    res.set_uid(get_uid());
    res.set_status(get_status());
    
    // Serialize and send response
    std::string response_payload;
    if (res.SerializeToString(&response_payload)) {
        publish_point_to_point_raw(req.uid(), "Trevor.HealthCheckResponse", response_payload);
        get_logger()->info("✅ Sent HealthCheckResponse to: {}", req.uid());
    } else {
        get_logger()->error("❌ Failed to serialize HealthCheckResponse");
    }
}
```

## ServiceHost API

### New Methods:
```cpp
class ServiceHost {
public:
    // Handler takes raw payload; your logic will parse it
    using HandlerRaw = std::function<void(const std::string& payload)>;

    // Map: message type name → (routing, handler)
    using RegistrationMap = std::unordered_map<std::string, std::pair<MessageRouting, HandlerRaw>>;

    // Batch-register handlers from a map
    void register_handlers(const RegistrationMap& regs);

    // Individual handler registration (alternative to map-based)
    void register_handler(const std::string& message_type, 
                         MessageRouting routing, 
                         HandlerRaw handler);

protected:
    // Protected access for derived classes
    natsConnection* get_nats_connection() { return conn_; }
};
```

## Benefits

### 1. **Simplified Development**
- No complex macros or templates
- Clear method names and purposes
- Easy to understand and maintain
- Straightforward testing

### 2. **Clean Architecture**
- Business logic separated from registration
- Raw payload handling allows flexibility
- Service-focused design
- Clear separation of concerns

### 3. **Better Developer Experience**
- Less boilerplate code
- Easier debugging
- Clear error messages
- Intuitive API

### 4. **Maintainability**
- Registration map is easy to read
- Handler methods are focused
- Clear method signatures
- Simple to add new handlers

## Migration Guide

### Step 1: Update Service Class
```cpp
// Add business logic methods
void onMessageTypeRaw(const std::string& payload);

// Add registration map method
ServiceHost::RegistrationMap get_handler_registrations();
```

### Step 2: Implement Handlers
```cpp
void MyService::onMessageTypeRaw(const std::string& payload) {
    // Parse protobuf
    MyMessage msg;
    if (!msg.ParseFromString(payload)) {
        get_logger()->error("Failed to parse message");
        return;
    }
    
    // Business logic
    process_message(msg);
}
```

### Step 3: Update main.cpp
```cpp
// Create service
MyService svc("service-001", config_file);

// Register handlers
svc.register_handlers(svc.get_handler_registrations());

// Initialize service
svc.initialize_service(config);
```

## Result

Your services now focus on **business logic** instead of **infrastructure complexity**. The registration system is clean, maintainable, and allows developers to concentrate on what matters most - implementing the core functionality of their microservices.

**The service implementation is now significantly simplified while maintaining all the power and flexibility of the Seven framework!** 🎯
