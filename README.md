# Seven - Modern C++ Microservices Framework

A production-ready, high-performance microservices framework built with C++17, NATS messaging, and thread pool architecture. Features template-based message routing, graceful shutdown, configuration management, and Docker containerization for scalable portfolio management services.

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
```

### Core Components

1. **ServiceHost** - Template-based message routing engine with RAII resource management
2. **ThreadPool** - Configurable parallel processing with graceful shutdown
3. **Configuration** - YAML-based config with live reload (full) or simplified fallback
4. **MessageRegistration** - Type-safe protobuf message handlers with routing policies
5. **Signal Handling** - SIGINT/SIGTERM graceful shutdown with resource cleanup

## 🚀 Key Features

### ✅ Template-Based Message System
- **Type-safe registration** - Compile-time message type validation with `MSG_REG` macro
- **Flexible routing** - Point-to-point and broadcast message distribution
- **Non-blocking processing** - NATS callbacks offload work to thread pool immediately
- **Protobuf serialization** - Efficient binary message format with schema validation

### ✅ Configuration Management
- **Dual-mode config** - Full YAML support with yaml-cpp or simplified fallback
- **Live reload** - File watching with inotify for configuration changes
- **Environment override** - Command-line config file specification
- **Build-time detection** - Automatic selection based on available dependencies

### ✅ Production Operations
- **Graceful shutdown** - Signal handlers ensure proper resource cleanup
- **Thread pool isolation** - Backpressure protection for NATS event loop
- **Docker ready** - Multi-stage builds with dependency management
- **Health monitoring** - Built-in health check endpoints with service status

### ✅ Performance & Scalability
- **Hardware concurrency** - Default thread count matches CPU cores
- **Parallel message processing** - Multiple handlers execute simultaneously
- **RAII resource management** - Automatic cleanup prevents memory leaks
- **Header-only templates** - Fast compilation with inline optimizations

### 🆕 Structured Logging System (NEW!)
- **Production logging** - spdlog/fmt with structured format or stdout fallback
- **Daily log rotation** - One file per day (`service_name_YYYY-MM-DD.log`) with backup rotation
- **Correlation IDs** - 8-character hex IDs for distributed request tracing
- **Dynamic log levels** - Environment `LOG_LEVEL` (TRACE|DEBUG|INFO|WARN|ERROR|CRITICAL)
- **Signal handling** - SIGHUP reloads log configuration from environment variables
- **Request tracking** - Child loggers inherit correlation, request loggers get fresh IDs
- **Performance metrics** - Automatic handler execution timing in DEBUG mode
- **Thread-safe integration** - Full ServiceHost and thread pool logging support

## 📁 Project Structure

```
Seven/
├── libs/common/                    # Core framework components
│   ├── service_host.hpp           # Main template-based service framework
│   ├── service_host_impl.cpp      # NATS integration and signal handling
│   ├── logger.hpp                 # 🆕 Structured logging with correlation IDs
│   ├── logger.cpp                 # 🆕 Static member definitions for logger
│   ├── thread_pool.hpp            # Configurable parallel processing
│   ├── configuration.hpp          # Full YAML configuration (requires yaml-cpp)
│   ├── configuration_simple.hpp   # Fallback configuration (no dependencies)
│   └── messages_register.hpp      # Message registration helpers
├── services/
│   └── portfolio_manager/         # Example financial service
│       ├── main.cpp               # Service entry point with message handlers
│       ├── portfolio_manager.hpp  # Service-specific wrapper class
│       └── CMakeLists.txt
├── proto/                         # Protocol buffer definitions
│   ├── messages.proto             # Financial service message schemas
│   └── generate_proto.sh
├── tests/                         # Unit and integration tests
│   ├── test_graceful_shutdown.cpp # Shutdown and config testing
│   └── test_service_host.cpp      # Core framework testing
├── scripts/                       # Build and deployment utilities
│   ├── build.sh                   # Standard build script
│   ├── docker_build_up.sh         # Docker compose build & run
│   └── clean.sh                   # Build artifact cleanup
├── CMakeLists.txt                 # Main build configuration
├── Dockerfile                     # Production container definition
├── docker-compose.yml             # Multi-service orchestration
└── config.yaml                    # Default service configuration
```

## 🛠️ Technical Implementation

### Message Registration Pattern
```cpp
// Template-based type-safe message registration
PortfolioManager svc(
    PortfolioManager::ConfigFileTag{},
    "service-001",
    "config.yaml",
    MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::HealthCheckRequest& req) {
            // Handler logic executed in thread pool
            Trevor::HealthCheckResponse res;
            res.set_service_name("PortfolioManager");
            svc.publish_point_to_point(req.uid(), res);
        }),
    MSG_REG(Trevor::PortfolioRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::PortfolioRequest& req) {
            // Parallel processing across CPU cores
            svc.submit_task([req]() {
                // Business logic here
            });
        })
);
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

## 🚦 Quick Start

### 1. Build (Local)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF
make -j$(nproc)
```

### 2. Build (Docker)
```bash
docker-compose up --build
```

### 3. Run Service
```bash
./services/portfolio_manager/portfolio_manager config.yaml
```

## 🔧 Build Dependencies

### Required
- **C++17 compiler** (GCC 11+ or Clang 12+)
- **CMake 3.16+** - Build system
- **Protocol Buffers** - Message serialization (libprotobuf-dev)
- **NATS C Client** - Built from source in Docker
- **Threads** - Standard library threading

### Optional
- **yaml-cpp** - Full configuration support (falls back to simplified config)
- **Catch2** - Unit testing framework (tests disabled in production builds)

## 📊 Current Implementation Status

### ✅ Completed Components
- [x] ServiceHost template framework with fold expressions
- [x] Thread pool with configurable size and graceful shutdown
- [x] NATS integration with JetStream support
- [x] Protobuf message serialization (HealthCheck, Portfolio, MarketData)
- [x] Signal handling (SIGINT/SIGTERM) with proper cleanup
- [x] Configuration system with yaml-cpp fallback
- [x] Docker multi-stage builds with dependency management
- [x] Template constructor disambiguation with tag dispatch

### 🔄 Active Development
- [ ] NATS server integration in docker-compose
- [ ] Python client testing framework
- [ ] Production logging and monitoring
- [ ] Performance benchmarking and optimization

## 🎯 Next Development Priorities

1. **NATS Integration** - Add NATS server to docker-compose for end-to-end testing
2. **Client Testing** - Complete Python client for service validation
3. **Monitoring** - Add structured logging and health metrics
4. **Performance** - Benchmark message throughput and latency optimization

---

*This framework provides a solid foundation for building high-performance, scalable microservices with modern C++ patterns, production-ready operations, and flexible deployment options.*
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