# Seven - Modern C++ Microservices Framework with OpenTelemetry Observability

A production-ready, high-performance microservices framework built with C++17, NATS messaging, OpenTelemetry distributed tracing, and structured logging. Features template-based message routing, graceful shutdown, configuration management, and complete observability stack for scalable portfolio management services.

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
```

## 🚀 Key Features

### ✅ Template-Based Message System
- **Type-safe registration** - Compile-time message type validation with `MSG_REG` macro
- **Flexible routing** - Point-to-point and broadcast message distribution
- **Non-blocking processing** - NATS callbacks offload work to thread pool immediately
- **Protobuf serialization** - Efficient binary message format with schema validation

### ✅ Production Observability Stack 🆕
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

# 📈 Check OTEL Collector status
docker compose logs -f otel-collector

# 🎯 View Jaeger traces
# Open http://localhost:16686 → Select "portfolio_manager" service

# 🔧 Debug service health
docker compose exec portfolio_manager ps aux
docker compose exec portfolio_manager netstat -tulpn

# 📋 Full system status
docker compose ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"
```

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

# 🔗 Test NATS connectivity
docker compose exec portfolio_manager nats-pub test.subject "hello"

# 📡 Verify OTEL Collector connectivity
curl http://localhost:8888/metrics | grep -i otel

# 🔍 Check OpenTelemetry integration
docker compose logs portfolio_manager | grep -i "opentelemetry\|trace_id"

# 📊 Monitor resource usage
docker stats seven-portfolio-manager seven-otel-collector seven-jaeger
```

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

### 3. Test the System
```bash
# Run observability verification
./verify-observability.sh

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

## 🔍 Observability Quick Reference

### Essential Commands
```bash
# Start everything
docker compose up -d

# View logs with traces
docker compose logs -f portfolio_manager | grep trace_id

# Access Jaeger UI
$BROWSER http://localhost:16686

# Check service health
curl http://localhost:8888/metrics

# Stop everything
docker compose down
```

### Key URLs
- **Jaeger UI**: http://localhost:16686 (Distributed tracing)
- **Portfolio Manager**: http://localhost:8080 (Main service)
- **OTEL Collector**: http://localhost:8888/metrics (Health)

### Environment Variables
```bash
OTEL_SERVICE_NAME=portfolio_manager
OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4317
SPDLOG_LEVEL=info
NATS_URL=nats://nats:4222
```

*This framework provides a complete production-ready observability stack with modern C++ patterns, OpenTelemetry distributed tracing, structured logging, and visual trace analysis for building scalable, monitorable microservices.*
                       │   Logic)         │
                       └──────────────────┘
```

## 🛠️ Quick Start

### Prerequisites
- Docker and Docker Compose
- C++17 compatible compiler
- CMake 3.16+
- NATS C client library
- Protocol Buffers

### Build and Run

```bash
# Clone the repository
git clone https://github.com/osvaldoroques/Seven.git
cd Seven

# Build and run with Docker
./scripts/docker_build_up.ps1  # Windows PowerShell
# or
./scripts/docker_build_up.sh   # Linux/macOS

# Build locally
mkdir build && cd build
cmake .. -GNinja
ninja

# Run tests
./tests/test_service_host
```

## 📝 Usage Example

### Creating a Service with Thread Pool

```cpp
#include "portfolio_manager.hpp"
#include "messages_register.hpp"
#include "messages.pb.h"

int main() {
    // ServiceHost with default thread pool (uses all CPU cores)
    PortfolioManager svc(
        "svc-portfolio-001",
        MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint,
            [&svc](const Trevor::HealthCheckRequest& req) {
                std::cout << "✅ Received HealthCheckRequest from: "
                          << req.service_name() << " UID: " << req.uid() << std::endl;

                Trevor::HealthCheckResponse res;
                res.set_service_name("PortfolioManager");
                res.set_uid(svc.get_uid());
                res.set_status("ok");
                svc.publish_point_to_point(req.uid(), res);

                std::cout << "✅ Sent HealthCheckResponse" << std::endl;
            })
        // Add more MSG_REG(...) here as needed
    );

    // Initialize NATS connection
    svc.init_nats("nats://nats:4222");
    svc.init_jetstream();
    
    std::cout << "✅ PortfolioManager is now running." << std::endl;

    // Keep service running
    while (true) {
        nats_Sleep(1000);
    }
}
```

### Custom Thread Pool Size

```cpp
// High-load service with 8 worker threads
ServiceHost svc("service-id", "service-name", 8, 
    MSG_REG(MessageType, MessageRouting::Broadcast, handler)
);

// Submit custom background tasks
svc.submit_task([]{ 
    // Background processing work
});
```

## 🐳 Docker Deployment

### Services
- **seven-nats-server** - NATS message broker with JetStream
- **seven-portfolio-manager** - C++ portfolio management service
- **seven-healthcheck-client** - Python health check client

### Container Health Checks
```yaml
services:
  nats:
    container_name: seven-nats-server
    healthcheck:
      test: ["CMD", "/nats-server", "--help"]
      interval: 2s
      timeout: 3s
      retries: 5
```

## 🧪 Testing

### Unit Tests
```bash
cd build
./tests/test_service_host
```

**Expected Output:**
```
✅ Registered handler for: Trevor.HealthCheckRequest
🔄 Processing Trevor.HealthCheckRequest in worker thread 140252267988544
===============================================================================
All tests passed (2 assertions in 1 test case)
```

### Integration Tests
```bash
# Generate protobuf files using Docker
./scripts/generate_proto_docker.ps1

# Full system test with Docker
docker-compose up
```

## 📊 Performance Benefits

| Feature | Benefit |
|---------|---------|
| **Thread Pool** | 🚀 Parallel message processing across CPU cores |
| **Non-blocking NATS** | ⚡ NATS threads stay responsive under load |
| **Backpressure Isolation** | 🛡️ Slow handlers don't affect message delivery |
| **Configurable Workers** | 🎛️ Tune performance based on workload |
| **Template-based** | 🔧 Zero-cost abstractions with type safety |

## 🔧 Configuration

### Thread Pool Sizing Guidelines
- **CPU-bound tasks**: `std::thread::hardware_concurrency()`
- **I/O-bound tasks**: `2-4x CPU cores`
- **Mixed workloads**: Start with CPU cores, monitor and adjust

### NATS Configuration
```cpp
// Environment-aware connection
const char* nats_url = std::getenv("NATS_URL");
if (!nats_url) {
    nats_url = "nats://localhost:4222";
}
svc.init_nats(nats_url);
```

## 🛠️ Development

### Project Structure
```
Seven/
├── libs/common/          # Shared ServiceHost and ThreadPool
├── services/             # Individual microservices
│   ├── portfolio_manager/ # C++ trading service
│   └── python_client/    # Python health check client
├── proto/               # Protocol Buffer definitions
├── tests/               # Unit tests
├── scripts/             # Build and deployment scripts
└── docker-compose.yml   # Container orchestration
```

### Building Components
```bash
# C++ services
ninja portfolio_manager

# Python protobuf generation
./scripts/generate_proto_docker.ps1

# All tests
ninja test_service_host
ctest
```

## 📈 Scalability

The thread pool architecture provides:
- **Horizontal scaling**: Multiple service instances
- **Vertical scaling**: Configurable worker threads per instance
- **Load isolation**: Independent message processing pipelines
- **Resource efficiency**: Optimal CPU and memory utilization

---

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes with tests
4. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.