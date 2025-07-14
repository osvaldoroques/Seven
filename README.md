# Seven - Modern C++ Microservices Framework with OpenTelemetry & Zero-Branching Performance

A production-ready, high-performance microservices framework built with C++17, NATS messaging, OpenTelemetry distributed tracing, structured logging, and advanced performance optimization. Features template-based message routing, graceful shutdown, configuration management, zero-branching function pointer optimization, and complete observability stack for scalable portfolio management services.

## 🏗️ Architecture Overview

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   NATS Server   │◄──►│   ServiceHost    │◄──►│  Thread Pool    │
│   (JetStream)   │    │  (Template-based │    │  (Configurable  │
│   Message       │    │   Message        │    │   Parallel      │
│   Broker        │    │   Routing)       │    │   Processing)   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐
                       │  Message         │
                       │  Handlers        │
                       │  (Business Logic)│
                       └──────────────────┘

              🔍 OBSERVABILITY STACK 🔍
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Jaeger UI      │◄──►│ OTEL Collector   │◄──►│ Portfolio Mgr   │
│  (Trace View)   │    │ (Telemetry Hub)  │    │ (Instrumented)  │
│  :16686         │    │ :4317/:4318      │    │ (Structured     │
└─────────────────┘    └──────────────────┘    │  Logs + Traces) │
                                               └─────────────────┘

              🚀 PERFORMANCE OPTIMIZATION 🚀
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Fast Path     │    │  Function        │    │  Traced Path    │
│ (Zero Overhead) │◄──►│  Pointers        │◄──►│ (Full Telemetry)│
│ 0.293μs/msg     │    │ (Zero Branching) │    │ 0.302μs/msg     │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## 🚀 Key Features

### ✅ Template-Based Message System
- **Type-safe registration** - Compile-time message type validation with `MSG_REG` macro
- **Flexible routing** - Point-to-point and broadcast message distribution
- **Non-blocking processing** - NATS callbacks offload work to thread pool immediately
- **Protobuf serialization** - Efficient binary message format with schema validation

### ✅ Zero-Branching Performance Optimization 🆕
- **Function Pointer Pattern** - Runtime switching between fast/traced implementations
- **Zero Overhead Mode** - Hot-path methods with no tracing penalty (0.293μs/msg)
- **Full Observability Mode** - Complete OpenTelemetry spans when needed (0.302μs/msg)
- **No If-Statements** - Function pointers eliminate branching in critical paths
- **Runtime Control** - Switch modes dynamically without recompilation

### ✅ Production Observability Stack
- **OpenTelemetry Integration** - Full C++ SDK with OTLP gRPC exporter
- **Distributed Tracing** - W3C Trace-Context propagation across services
- **Structured Logging** - JSON logs with trace_id/span_id correlation
- **Jaeger UI** - Visual trace analysis and distributed request flow
- **OTEL Collector** - Centralized telemetry processing and export

### ✅ Advanced Logging System
- **Production logging** - spdlog/fmt with structured format or stdout fallback
- **Daily log rotation** - One file per day (`service_name_YYYY-MM-DD.log`) with backup rotation
- **Correlation IDs** - 16-char trace_id + 8-char span_id for distributed request tracing
- **Dynamic log levels** - Environment `SPDLOG_LEVEL` (trace|debug|info|warn|error|critical)
- **Signal handling** - SIGHUP reloads log configuration from environment variables
- **Request tracking** - Child loggers inherit correlation, request loggers get fresh IDs
- **Performance metrics** - Automatic handler execution timing in DEBUG mode
- **Thread-safe integration** - Full ServiceHost and thread pool logging support

### ✅ Performance & Scalability
- **Hardware concurrency** - Default thread count matches CPU cores
- **Parallel message processing** - Multiple handlers execute simultaneously
- **RAII resource management** - Automatic cleanup prevents memory leaks
- **Header-only templates** - Fast compilation with inline optimizations

## 🔍 Observability Stack Commands & Endpoints

### 🚀 Quick Start Commands

```bash
# Build and start complete observability stack
docker compose build
docker compose up -d

# Check all services status
docker compose ps

# View structured logs with trace correlation
docker compose logs -f portfolio_manager

# Stop the entire stack
docker compose down
```

### 🌐 Service Endpoints

| Service | URL | Purpose |
|---------|-----|---------|
| **Portfolio Manager** | http://localhost:8080 | Main C++ service with OpenTelemetry |
| **Jaeger UI** | http://localhost:16686 | Distributed tracing visualization |
| **OTEL Collector (gRPC)** | http://localhost:4317 | OpenTelemetry gRPC receiver |
| **OTEL Collector (HTTP)** | http://localhost:4318 | OpenTelemetry HTTP receiver |
| **OTEL Collector Metrics** | http://localhost:8888/metrics | Collector health & performance |
| **NATS Server** | http://localhost:4222 | NATS messaging broker |
| **NATS Monitoring** | http://localhost:8222 | NATS server monitoring |

### 📊 Monitoring & Debugging Commands

```bash
# 🔍 View service logs with trace correlation
docker compose logs -f portfolio_manager | grep -E "(trace_id|span_id|ERROR|WARN)"

# 📈 Check OTEL Collector status (should show "Everything is ready")
docker compose logs -f otel-collector

# ✅ Verify OTEL Collector is receiving data
docker compose logs otel-collector | grep -E "(Starting GRPC server|Starting HTTP server|Everything is ready)"

# 🎯 View Jaeger traces
# Open http://localhost:16686 → Select "portfolio_manager" service

# 🔧 Debug service health
docker compose exec portfolio_manager ps aux
docker compose exec portfolio_manager netstat -tulpn

# 📋 Full system status
docker compose ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"
```

### 🔧 OTEL Collector Configuration (Updated)

The OTEL Collector has been updated to use modern, non-deprecated exporters:

```yaml
# ✅ Updated Configuration (config/otel-collector-config.yaml)
exporters:
  # OTLP exporter for Jaeger (replaces deprecated jaeger exporter)
  otlp/jaeger:
    endpoint: jaeger:14250
    tls:
      insecure: true
  
  # Debug exporter (replaces deprecated logging exporter)
  debug:
    verbosity: detailed

processors:
  memory_limiter:
    limit_mib: 128
    check_interval: 1s  # Required parameter
```

**Key Updates:**
- ❌ **Removed deprecated `jaeger` exporter** → ✅ **Using `otlp/jaeger`**
- ❌ **Removed deprecated `logging` exporter** → ✅ **Using `debug` exporter**
- ✅ **Added required `check_interval`** to memory_limiter processor

### 🛠️ Development & Testing Commands

```bash
# 🏗️ Build only specific service
docker compose build portfolio_manager

# 🧪 Test build with verification
.\test-otel-fix.bat                    # Windows
./test-otel-fix.sh                     # Linux/macOS

# 🔍 Verify observability features
.\verify-observability.sh              # Check endpoints & logs

# 🔄 Restart single service
docker compose restart portfolio_manager

# 📁 Access service container
docker compose exec portfolio_manager /bin/bash
```

### 🐛 Troubleshooting Commands

```bash
# 🚨 Check for service errors
docker compose logs portfolio_manager | grep -i error

# � Verify OTEL Collector is working properly
docker compose logs otel-collector | grep -E "(Everything is ready|Starting.*server)"

# �🔗 Test NATS connectivity
docker compose exec portfolio_manager nats-pub test.subject "hello"

# 📡 Verify OTEL Collector health endpoint
curl http://localhost:8888/metrics

# 🔍 Check OpenTelemetry integration
docker compose logs portfolio_manager | grep -i "opentelemetry\|trace_id"

# 📊 Monitor resource usage
docker stats seven-portfolio-manager seven-otel-collector seven-jaeger

# 🚨 Common OTEL Collector Issues
docker compose logs otel-collector | grep -i "error\|failed\|invalid"
```

### ⚠️ Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| **`jaeger exporter unknown type`** | ✅ Fixed: Using `otlp/jaeger` exporter |
| **`logging exporter deprecated`** | ✅ Fixed: Using `debug` exporter |
| **`check_interval must be greater than zero`** | ✅ Fixed: Added `check_interval: 1s` |
| **OTEL Collector not starting** | Check logs: `docker compose logs otel-collector` |
| **No traces in Jaeger** | Verify portfolio manager is sending traces |
| **Port conflicts** | Ensure ports 4317, 4318, 16686 are available |

### 🌟 Environment Variables

```bash
# Service Configuration
NATS_URL=nats://nats:4222                              # NATS connection
SPDLOG_LEVEL=info                                      # Log level (trace|debug|info|warn|error|critical)

# OpenTelemetry Configuration  
OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4317 # OTLP gRPC endpoint
OTEL_SERVICE_NAME=portfolio_manager                     # Service name in traces
OTEL_RESOURCE_ATTRIBUTES=service.name=portfolio_manager,service.version=1.0.0
```

## 📁 Project Structure with Observability

```
Seven/
├── libs/common/                    # Core framework components
│   ├── service_host.hpp           # Main template-based service framework
│   ├── service_host_impl.cpp      # NATS integration and signal handling
│   ├── logger.hpp                 # 🆕 Structured logging with correlation IDs & OpenTelemetry
│   ├── opentelemetry_integration.hpp # 🆕 OpenTelemetry C++ SDK wrapper
│   ├── opentelemetry_integration.cpp # 🆕 OTLP exporter & W3C trace context
│   ├── thread_pool.hpp            # Configurable parallel processing
│   ├── configuration.hpp          # Full YAML configuration (requires yaml-cpp)
│   ├── configuration_simple.hpp   # Fallback configuration (no dependencies)
│   └── messages_register.hpp      # Message registration helpers
├── services/
│   └── portfolio_manager/         # Example financial service
│       ├── main.cpp               # Service entry point with message handlers
│       ├── portfolio_manager.hpp  # Service-specific wrapper class
│       └── CMakeLists.txt
├── config/                        # 🆕 Observability Configuration
│   └── otel-collector-config.yaml # OpenTelemetry Collector pipeline config
├── proto/                         # Protocol buffer definitions
│   ├── messages.proto             # Financial service message schemas
│   └── generate_proto.sh
├── tests/                         # Unit and integration tests
│   ├── test_graceful_shutdown.cpp # Shutdown and config testing
│   └── test_service_host.cpp      # Core framework testing
├── scripts/                       # Build and deployment utilities
│   ├── build.sh                   # Standard build script
│   ├── test-build.sh              # 🆕 Observability stack test script
│   ├── test-otel-fix.bat          # 🆕 Windows OpenTelemetry build test
│   └── verify-observability.sh    # 🆕 Service health & endpoint verification
├── CMakeLists.txt                 # Main build configuration with OpenTelemetry
├── Dockerfile                     # Production container with OpenTelemetry C++ SDK
├── docker-compose.yml             # 🆕 Full observability stack orchestration
└── config.yaml                    # Default service configuration
```

## 🛠️ Technical Implementation with OpenTelemetry

### Message Registration with Distributed Tracing
```cpp
// Template-based type-safe message registration with OpenTelemetry spans
PortfolioManager svc(
    PortfolioManager::ConfigFileTag{},
    "service-001",
    "config.yaml",
    MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::HealthCheckRequest& req) {
            // Automatic span creation and trace context propagation
            auto span = OpenTelemetryIntegration::start_span("handle_health_check");
            
            Trevor::HealthCheckResponse res;
            res.set_service_name("PortfolioManager");
            svc.publish_point_to_point(req.uid(), res);
            
            // Span automatically ends with RAII
        }),
    MSG_REG(Trevor::PortfolioRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::PortfolioRequest& req) {
            // Parallel processing with trace correlation
            svc.submit_task([req]() {
                auto span = OpenTelemetryIntegration::start_span("process_portfolio");
                // Business logic with automatic trace_id in logs
            });
        })
);
```

### Structured Logging with Trace Correlation
```cpp
#include "logger.hpp"

// Logger automatically includes trace_id and span_id
Logger::info("Processing request", {
    {"user_id", user_id},
    {"portfolio_size", portfolio.size()},
    {"request_time", std::chrono::system_clock::now()}
});

// Output: {"timestamp":"2024-01-15T10:30:45.123Z","level":"info","message":"Processing request","trace_id":"a1b2c3d4e5f6789a","span_id":"1a2b3c4d","user_id":"user123","portfolio_size":5}
```

### OpenTelemetry Integration
```cpp
#include "opentelemetry_integration.hpp"

// Initialize OpenTelemetry with OTLP exporter
OpenTelemetryIntegration::initialize("portfolio_manager", "http://otel-collector:4317");

// Create spans with W3C trace context
{
    auto span = OpenTelemetryIntegration::start_span("database_query");
    span.set_attribute("table", "portfolios");
    span.set_attribute("query_duration_ms", 45.2);
    // Span automatically propagates to logs
} // Span ends here with RAII
```

### Configuration System
```cpp
// Automatic fallback: yaml-cpp if available, simplified otherwise
#ifdef HAVE_YAML_CPP
#include "configuration.hpp"     // Full YAML with live reload
#else
#include "configuration_simple.hpp"  // Hardcoded defaults
#endif

// Template-based config access
auto nats_url = config_.get<std::string>("nats.url", "nats://localhost:4222");
auto thread_count = config_.get<size_t>("threads", std::thread::hardware_concurrency());
```

### Thread Pool Integration
```cpp
// NATS callback immediately returns, work offloaded to pool
void onMessage(const std::string& subject, const std::string& data) {
    thread_pool_.submit([this, subject, data]() {
        // Heavy processing in background thread
        processMessage(subject, data);
    });
    // NATS thread stays responsive
}
```

## 🚦 Quick Start with Observability

### 1. Build Complete Stack (Docker - Recommended)
```bash
# Clone repository
git clone https://github.com/osvaldoroques/Seven.git
cd Seven

# Build and start full observability stack
docker compose build
docker compose up -d

# Verify all services are running
docker compose ps
```

### 2. Access Observability Interfaces
```bash
# Open Jaeger UI for distributed tracing
$BROWSER http://localhost:16686

# View structured logs with trace correlation
docker compose logs -f portfolio_manager

# Check OTEL Collector metrics
curl http://localhost:8888/metrics
```

### 3. Test the System & Performance Demo
```bash
# Run observability verification
./verify-observability.sh

# 🚀 Performance Demo: Run portfolio manager locally to see function pointer optimization
cd build && ./services/portfolio_manager/portfolio_manager

# Output shows zero-branching performance comparison:
# 📊 Test 1: High-Performance Mode (Fast Functions)
#    • 1000 messages published in: 293μs
#    • Average per message: 0.293μs
# 📊 Test 2: Full Observability Mode (Traced Functions)  
#    • 1000 messages published in: 302μs
#    • Average per message: 0.302μs
# 🎯 Performance Analysis:
#    • Tracing overhead ratio: 1.03x
#    • Runtime switching: ZERO branching penalty!

# Generate test traces (if client is available)
# Traces will appear in Jaeger UI at http://localhost:16686
```

### 4. Build Locally (Development)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF
make -j$(nproc)

# Run with local NATS (requires NATS server)
./services/portfolio_manager/portfolio_manager config.yaml
```

## 🔧 Build Dependencies with OpenTelemetry

### Required
- **C++17 compiler** (GCC 11+ or Clang 12+)
- **CMake 3.16+** - Build system
- **Protocol Buffers** - Message serialization (libprotobuf-dev)
- **NATS C Client** - Built from source in Docker
- **Threads** - Standard library threading
- **OpenTelemetry C++ SDK** - Distributed tracing (built automatically in Docker)
- **gRPC** - Required for OTLP exporter (libgrpc++-dev)
- **cURL** - HTTP client for OpenTelemetry (libcurl4-openssl-dev)

### Optional (Auto-detected)
- **spdlog/fmt** - Structured logging (falls back to stdout)
- **yaml-cpp** - Full configuration support (falls back to simplified config)
- **Catch2** - Unit testing framework (tests disabled in production builds)

### Docker Dependencies (Automatically Installed)
```dockerfile
# System dependencies for OpenTelemetry
libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev 
libabsl-dev libre2-dev libgrpc++-dev libprotobuf-dev

# OpenTelemetry C++ SDK v1.9.1 (built from source)
# Configured with OTLP gRPC exporter and W3C trace context
```

## 📊 Implementation Status with Observability

### ✅ Completed Components
- [x] **ServiceHost** template framework with fold expressions
- [x] **Thread pool** with configurable size and graceful shutdown
- [x] **NATS integration** with JetStream support and environment configuration
- [x] **Protobuf** message serialization (HealthCheck, Portfolio, MarketData)
- [x] **Signal handling** (SIGINT/SIGTERM) with proper cleanup
- [x] **Configuration system** with yaml-cpp fallback
- [x] **Docker** multi-stage builds with dependency management
- [x] **🆕 OpenTelemetry C++ SDK** - Full integration with OTLP gRPC exporter
- [x] **🆕 Distributed Tracing** - W3C Trace-Context propagation
- [x] **🆕 Structured Logging** - JSON logs with trace_id/span_id correlation
- [x] **🆕 Jaeger Integration** - Visual trace analysis and debugging
- [x] **🆕 OTEL Collector** - Centralized telemetry processing pipeline

### 🔄 Active Development
- [ ] **Performance benchmarking** with OpenTelemetry metrics
- [ ] **Custom metrics** export via OpenTelemetry
- [ ] **Distributed context** propagation through NATS messages
- [ ] **Alerting integration** with Prometheus/Grafana

## 🎯 Observability Features in Action

### What You Get Out of the Box

1. **🔍 Distributed Tracing**
   - Automatic span creation for message handlers
   - W3C Trace-Context propagation across services
   - Visual request flow analysis in Jaeger UI
   - Performance bottleneck identification

2. **📋 Structured Logging** 
   - JSON-formatted logs with consistent schema
   - Automatic trace_id/span_id injection
   - Daily log rotation with configurable retention
   - Environment-based log level control

3. **📊 Service Monitoring**
   - OTEL Collector health metrics at :8888/metrics
   - Container health checks and restart policies
   - Resource usage monitoring via Docker stats
   - Service dependency health verification

4. **🚀 Production Ready**
   - Zero-downtime deployments with health checks
   - Graceful shutdown with trace completion
   - Environment-based configuration override
   - Multi-stage Docker builds for optimal image size

## 🎯 Next Development Priorities

1. **Custom Metrics Integration** - Add business metrics export via OpenTelemetry
2. **Message Tracing** - Implement W3C trace context in NATS message headers
3. **Performance Analytics** - Service latency and throughput dashboards
4. **Alerting Setup** - Integration with Prometheus/Grafana for production monitoring

---

## 🔍 Observability Quick Reference (Updated)

### Essential Commands
```bash
# Start everything
docker compose up -d

# ✅ Verify OTEL Collector started successfully
docker compose logs otel-collector | grep "Everything is ready"

# View logs with traces
docker compose logs -f portfolio_manager | grep trace_id

# Access Jaeger UI
$BROWSER http://localhost:16686

# Check collector health
curl http://localhost:8888/metrics

# Stop everything
docker compose down
```

### Key URLs
- **Jaeger UI**: http://localhost:16686 (Distributed tracing - select "portfolio_manager" service)
- **Portfolio Manager**: http://localhost:8080 (Main service)
- **OTEL Collector**: http://localhost:8888/metrics (Health & metrics)
- **OTEL gRPC**: http://localhost:4317 (Telemetry receiver)
- **OTEL HTTP**: http://localhost:4318 (HTTP telemetry receiver)

### Environment Variables
```bash
OTEL_SERVICE_NAME=portfolio_manager
OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4317
SPDLOG_LEVEL=info
NATS_URL=nats://nats:4222
```

### ✅ Recent Updates (July 2025)
- **Fixed OTEL Collector** configuration with modern exporters
- **Replaced deprecated** `jaeger` exporter with `otlp/jaeger`
- **Replaced deprecated** `logging` exporter with `debug`
- **Added required** `check_interval` to memory_limiter processor
- **Verified compatibility** with OpenTelemetry Collector v0.129.0

*This framework provides a complete production-ready observability stack with modern C++ patterns, OpenTelemetry distributed tracing, structured logging, and visual trace analysis for building scalable, monitorable microservices.*

## 🎯 Advanced Usage: Performance Optimization

### Function Pointer Hot-Path Optimization

The Seven framework provides runtime switching between high-performance and full-observability modes:

```cpp
#include "service_host.hpp"

int main() {
    ServiceHost service("my-service", "MyService");
    
    // Initialize NATS
    service.init_nats("nats://localhost:4222");
    
    // 🚀 Performance Mode: Zero overhead for high-throughput scenarios
    service.disable_tracing();
    
    // Hot-path operations run at maximum speed (0.293μs per message)
    for (int i = 0; i < 1000000; ++i) {
        service.publish_broadcast(message);  // Uses fast implementation
    }
    
    // 🔍 Observability Mode: Full tracing for debugging/monitoring
    service.enable_tracing();
    
    // Operations include full OpenTelemetry spans (0.302μs per message)
    service.publish_broadcast(debug_message);  // Uses traced implementation
    
    // Dynamic mode checking
    if (service.is_tracing_enabled()) {
        logger->info("Running with full observability");
    } else {
        logger->info("Running in high-performance mode");
    }
}
```

### Configuration-Based Performance Control

```cpp
// Switch modes based on configuration
auto performance_mode = config.get<std::string>("performance.mode", "traced");
if (performance_mode == "fast") {
    service.disable_tracing();
    logger->warn("High-performance mode: Tracing disabled");
} else {
    service.enable_tracing();
    logger->info("Full observability mode: Tracing enabled");
}
```

### Environment-Based Runtime Control

```cpp
// Control via environment variables
const char* perf_mode = std::getenv("PERFORMANCE_MODE");
if (perf_mode && std::string(perf_mode) == "FAST") {
    service.disable_tracing();
    std::cout << "🚀 High-performance mode enabled via PERFORMANCE_MODE=FAST" << std::endl;
} else {
    service.enable_tracing();
    std::cout << "🔍 Full observability mode (default)" << std::endl;
}
```