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

1. Add NATS server to docker-compose for end-to-end testing
2. Complete Python client integration testing  
3. Add structured logging and health metrics
4. Performance optimization and benchmarking

## Critical Implementation Notes

- **Constructor ambiguity**: Always use `ConfigFileTag{}` when passing config file to PortfolioManager
- **Dependencies**: yaml-cpp is optional - build system automatically falls back
- **Thread safety**: NATS callbacks offload to thread pool immediately - no blocking operations
- **Resource cleanup**: RAII pattern ensures proper shutdown sequence
- **Message types**: All handlers use Trevor:: namespace protobuf messages
