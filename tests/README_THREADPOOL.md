# ThreadPool Test Suite

This directory contains comprehensive tests for the ThreadPool implementation in the Seven framework, designed to validate thread safety, performance, and integration with the messaging system.

## Test Files

### 📋 `test_thread_pool.cpp`
**Core functionality tests**
- Basic ThreadPool operations (submit, shutdown, size)
- Different thread count configurations
- Exception safety in task execution
- Move semantics and RAII behavior
- Concurrent submissions and stress testing
- Copy/move semantics validation

### 🔗 `test_thread_pool_integration.cpp`
**Integration with Seven framework**
- ThreadPool + ServiceHost message processing
- High-load performance testing
- OpenTelemetry tracing integration
- Graceful shutdown during active processing
- Error handling in distributed scenarios

### 🛡️ `test_thread_pool_sanitizers.cpp`
**Sanitizer-specific validation**
- Memory access pattern testing (AddressSanitizer)
- Data race detection scenarios (ThreadSanitizer)  
- Integer overflow prevention (UBSan)
- Memory initialization patterns (MemorySanitizer)
- Rapid lifecycle and concurrent operations
- Exception safety under sanitizer monitoring

## Running Tests

### Quick Test Run
```bash
# Build and run all ThreadPool tests
cd build
ninja test_thread_pool test_thread_pool_integration test_thread_pool_sanitizers
ctest --output-on-failure -R "thread_pool"
```

### Comprehensive Sanitizer Testing
```bash
# Run tests with all sanitizer configurations
./scripts/test-threadpool.sh
```

### Individual Test Suites
```bash
# Basic functionality only
./build/test_thread_pool

# Integration tests only  
./build/test_thread_pool_integration

# Sanitizer-specific tests only
./build/test_thread_pool_sanitizers
```

## Sanitizer Configurations

### AddressSanitizer + UBSan
```bash
cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
export ASAN_OPTIONS="verbosity=1:abort_on_error=1:detect_leaks=1"
export UBSAN_OPTIONS="print_stacktrace=1:abort_on_error=1"
```

### ThreadSanitizer
```bash
cmake .. -DENABLE_TSAN=ON
export TSAN_OPTIONS="verbosity=1:abort_on_error=1:halt_on_error=1"
```

### MemorySanitizer
```bash
cmake .. -DENABLE_MSAN=ON
export MSAN_OPTIONS="verbosity=1:abort_on_error=1"
```

## Test Coverage

### 🔒 Thread Safety
- ✅ Concurrent task submission
- ✅ Simultaneous pool queries during processing
- ✅ Race condition prevention in shutdown
- ✅ Atomic operations validation
- ✅ Lock contention under high load

### 🛡️ Memory Safety  
- ✅ Stack-use-after-scope prevention
- ✅ Heap buffer overflow detection
- ✅ Use-after-free prevention
- ✅ Memory leak detection
- ✅ Proper RAII resource management

### ⚡ Performance
- ✅ High-throughput message processing (>1000 msg/sec)
- ✅ Low-latency task execution (<10ms average)
- ✅ Efficient worker utilization
- ✅ Minimal lock contention
- ✅ Stress testing with 10,000+ tasks

### 🔗 Integration
- ✅ ServiceHost + ThreadPool messaging
- ✅ OpenTelemetry tracing compatibility
- ✅ Exception isolation and recovery
- ✅ Graceful shutdown coordination
- ✅ NATS message processing pipeline

### 🧪 Edge Cases
- ✅ Zero thread pool handling
- ✅ Task submission after shutdown
- ✅ Exception handling in tasks
- ✅ Rapid pool creation/destruction
- ✅ Move semantics during active processing

## Expected Test Results

### Performance Benchmarks
- **Basic throughput**: >100 msg/sec
- **High-load throughput**: >1000 msg/sec  
- **Average latency**: <10ms per message
- **Stress test**: 10,000 tasks in <5 seconds

### Sanitizer Results
- **AddressSanitizer**: No memory safety violations
- **ThreadSanitizer**: No data races or deadlocks
- **UBSan**: No undefined behavior
- **MemorySanitizer**: No uninitialized memory access

## Troubleshooting

### Test Failures
1. **Timeout issues**: Increase wait times in tests for slower systems
2. **Performance variations**: Benchmarks may vary based on hardware
3. **Sanitizer slowdown**: Tests run slower with sanitizers (expected)

### Common Issues
- **TSan false positives**: Review suppression files if needed
- **ASan memory usage**: Ensure sufficient memory for tests
- **Build errors**: Check CMake configuration and dependencies

## Integration with CI/CD

```yaml
# Example GitHub Actions configuration
- name: Build ThreadPool Tests
  run: |
    mkdir build && cd build
    cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
    ninja test_thread_pool test_thread_pool_integration

- name: Run ThreadPool Tests
  run: |
    cd build
    export ASAN_OPTIONS="abort_on_error=1"
    ctest --output-on-failure -R "thread_pool"
```

The ThreadPool test suite ensures robust, thread-safe, and performant concurrent task execution in the Seven framework! 🚀
