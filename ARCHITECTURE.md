# Seven Framework - Architecture Summary

## Quick Implementation Reference

### Core Pattern: Template-Based Service Framework
```cpp
// ServiceHost = Template engine for type-safe message routing
// ThreadPool = Configurable parallel processing 
// Configuration = YAML (full) or simplified fallback
// MSG_REG = Macro for compile-time message registration
```

### Key Design Decisions (for future reference)

1. **Template Constructor Disambiguation** 
   - Problem: `PortfolioManager(uid, config_file, msg_regs...)` conflicted with `PortfolioManager(uid, msg_regs...)`
   - Solution: Tag dispatch with `ConfigFileTag{}` to force specific constructor selection
   - Location: `portfolio_manager.hpp:16-20`

2. **Configuration Fallback System**
   - Full: `configuration.hpp` (requires yaml-cpp dependency)
   - Fallback: `configuration_simple.hpp` (hardcoded defaults)
   - Selection: `#ifdef HAVE_YAML_CPP` conditional compilation
   - CMake: `pkg_check_modules(YAMLCPP yaml-cpp)` sets HAVE_YAML_CPP flag

3. **Thread Pool Integration** 
   - NATS callbacks immediately submit work to thread pool and return
   - Prevents blocking NATS event loop under heavy processing load
   - Configurable pool size: `config.get<size_t>("threads", hardware_concurrency())`

4. **Message Registration Flow**
   ```cpp
   MSG_REG(MessageType, RoutingPolicy, Handler) 
   → MessageRegistration<MessageType> object
   → .Register(ServiceHost*) called in fold expression
   → Stores handler in unordered_map<subject, callback>
   ```

## Current Compilation Status ✅

- **Build System**: CMake with conditional yaml-cpp support
- **Dependencies**: Protobuf (required), yaml-cpp (optional), NATS C client (built from source)
- **Template Issues**: Resolved constructor ambiguity with tag dispatch
- **Configuration**: Working with simplified fallback when yaml-cpp unavailable
- **Threading**: Thread pool operational with graceful shutdown
- **Signals**: SIGINT/SIGTERM handlers registered for clean shutdown

## File Organization

### Core Framework (`libs/common/`)
- `service_host.hpp` - Main template framework (222 lines)
- `service_host_impl.cpp` - NATS integration & signal handling
- `thread_pool.hpp` - Parallel processing with shutdown
- `configuration.hpp` / `configuration_simple.hpp` - Config management
- `messages_register.hpp` - Registration helpers and MSG_REG macro

### Service Implementation (`services/portfolio_manager/`)
- `main.cpp` - Example service with HealthCheck, Portfolio, MarketData handlers
- `portfolio_manager.hpp` - Service wrapper with ConfigFileTag disambiguation

### Protocol & Testing
- `proto/messages.proto` - Financial service message schemas
- `tests/` - Graceful shutdown and framework testing

## Last Working State

✅ **Compilation**: Complete success with simplified configuration
✅ **Execution**: Service starts, registers handlers, attempts NATS connection
✅ **Configuration**: Loads config.yaml, shows default values  
✅ **Threading**: Thread pool initialized with hardware concurrency
✅ **Cleanup**: Signal handlers installed for graceful shutdown

## Next Implementation Steps

1. **🆕 Structured Logging Integration** - spdlog with correlation IDs and dynamic log levels ✅
2. Add NATS server to docker-compose for end-to-end testing
3. Complete Python client integration testing  
4. Add structured logging and health metrics
5. Performance optimization and benchmarking

## Critical Implementation Notes

- **Constructor ambiguity**: Always use `ConfigFileTag{}` when passing config file to PortfolioManager
- **🆕 Logging system**: spdlog (full) or stdout fallback, with correlation IDs and env LOG_LEVEL
- **🆕 Daily log rotation**: Requires spdlog/fmt dependencies for file output (console always works)
- **Dependencies**: yaml-cpp is optional - build system automatically falls back
- **Thread safety**: NATS callbacks offload to thread pool immediately - no blocking operations

## 🔧 Dependency Status & Installation

### Current Dev Container Status
```bash
CMake Warning: spdlog/fmt not found - logging will use simple stdout
CMake Warning: yaml-cpp not found - using simplified configuration
```

### To Enable Full Logging with Daily Rotation
```bash
# Option 1: Rebuild dev container (recommended)
# Update .devcontainer/Dockerfile already includes dependencies
# Ctrl+Shift+P -> "Dev Containers: Rebuild Container"

# Option 2: Manual installation (if possible)
sudo apt install libspdlog-dev libfmt-dev libyaml-cpp-dev
```

### Feature Matrix by Dependencies
| Feature | No Deps | spdlog/fmt | yaml-cpp |
|---------|---------|------------|----------|
| Console Logging | ✅ | ✅ | ✅ |
| Daily Log Files | ❌ | ✅ | ✅ |
| Structured Logs | ✅ (basic) | ✅ (full) | ✅ |
| Config Files | ✅ (simple) | ✅ | ✅ (full) |
| Correlation IDs | ✅ | ✅ | ✅ |
- **Resource cleanup**: RAII pattern ensures proper shutdown sequence
- **Message types**: All handlers use Trevor:: namespace protobuf messages

## 🆕 Logging System Integration

### Features Implemented
- **Structured Logging**: spdlog with fmt formatting (when available) or simple stdout fallback
- **Daily Log Rotation**: One file per day with timestamp in filename (`service_name_YYYY-MM-DD.log`)
- **Backup Rotation**: Additional 50MB rotating files with 5 backup copies for high-volume scenarios
- **Correlation IDs**: 8-character hex IDs for request tracing across service boundaries  
- **Dynamic Log Levels**: Environment variable `LOG_LEVEL` (TRACE|DEBUG|INFO|WARN|ERROR|CRITICAL)
- **Signal Handling**: SIGHUP reloads log level from environment
- **Request Tracing**: Child loggers inherit correlation ID, new request loggers generate fresh IDs
- **Performance Metrics**: Automatic handler execution timing in DEBUG mode
- **Thread Pool Integration**: Each message handler gets request-scoped logger

### Usage Examples
```cpp
// Service-level logging with correlation ID
auto logger = svc.get_logger();
logger->info("Service started with {} threads", thread_count);

// Request-scoped logging with new correlation ID
auto request_logger = svc.create_request_logger();
request_logger->debug("Processing request for account: {}", account_id);

// Error handling with correlation tracking
try {
    process_portfolio(req);
} catch (const std::exception& e) {
    request_logger->error("Portfolio processing failed: {}", e.what());
}
```

### Log Format
```
# Daily rotation files (when spdlog available)
logs/PortfolioManager_2025-07-12.log
logs/PortfolioManager_current.log (rotating backup)

# Console and file format
[2025-07-12 03:59:19.325] [INFO] [ServiceName] [correlation_id] Message content
```

### Log File Organization
- **Daily Files**: `logs/{service_name}_{YYYY-MM-DD}.log` - One file per day, rotated at midnight
- **Current Files**: `logs/{service_name}_current.log` - Size-based rotation (50MB, 5 backups)
- **Console Output**: Always enabled for real-time monitoring
- **Fallback Mode**: stdout only when spdlog unavailable
