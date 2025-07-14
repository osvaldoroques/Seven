# Sanitizer Integration Guide

This document explains how to use the integrated sanitizers in the Seven framework for detecting memory issues, undefined behavior, and concurrency bugs.

## Available Sanitizers

### 🔍 AddressSanitizer (ASan)
**Detects:**
- Buffer overflows and underflows
- Use-after-free bugs
- Use-after-return bugs
- Memory leaks
- Double-free bugs

### 🔍 UndefinedBehaviorSanitizer (UBSan)
**Detects:**
- Integer overflow/underflow
- Null pointer dereferences
- Invalid enum values
- Array bounds violations
- Type confusion

### 🧵 ThreadSanitizer (TSan)
**Detects:**
- Data races
- Thread safety violations
- Deadlocks (partial support)

### 🧠 MemorySanitizer (MSan)
**Detects:**
- Uninitialized memory reads

## Build Configurations

### Development Build (Default Debug)
```bash
# Automatically enables ASan + UBSan in Debug mode
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
ninja
```

### Manual Sanitizer Control
```bash
# Enable specific sanitizers
cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_TSAN=OFF

# Disable all sanitizers
cmake .. -DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF -DENABLE_TSAN=OFF
```

### Quick Build Scripts
```bash
# ASan + UBSan
./scripts/build-sanitizers.sh

# ThreadSanitizer only
./scripts/build-tsan.sh

# Production build (no sanitizers)
./scripts/build-release.sh
```

## Docker Development

### Standard Development with ASan + UBSan
```bash
docker compose -f docker-compose.yml -f docker-compose.dev.yml up
```

### ThreadSanitizer Testing
```bash
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile tsan up portfolio_manager_tsan
```

## Environment Variables

### AddressSanitizer Options
```bash
export ASAN_OPTIONS="verbosity=1:abort_on_error=1:check_initialization_order=1:detect_leaks=1"
```

### UBSan Options  
```bash
export UBSAN_OPTIONS="print_stacktrace=1:abort_on_error=1"
```

### ThreadSanitizer Options
```bash
export TSAN_OPTIONS="verbosity=1:abort_on_error=1:halt_on_error=1"
```

## Best Practices

### 1. Development Workflow
- Use ASan + UBSan during development
- Run TSan periodically for concurrency testing
- Use production builds for performance testing

### 2. CI/CD Integration
```yaml
# Example GitHub Actions step
- name: Build with Sanitizers
  run: |
    mkdir build && cd build
    cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
    ninja
    
- name: Run Tests with Sanitizers
  run: |
    cd build
    export ASAN_OPTIONS="abort_on_error=1"
    ctest --output-on-failure
```

### 3. Performance Impact
- **ASan**: ~2x memory usage, ~2x slower
- **UBSan**: ~20% slower, minimal memory impact
- **TSan**: ~5-15x slower, ~5-10x memory usage
- **MSan**: ~3x slower, ~3x memory usage

### 4. Sanitizer Conflicts
- **Cannot combine**: ASan + TSan
- **Cannot combine**: ASan + MSan  
- **Cannot combine**: TSan + MSan
- **Can combine**: ASan + UBSan ✅
- **Can combine**: TSan + UBSan ✅

## Troubleshooting

### Common Issues

#### False Positives
```bash
# Suppress known issues
export ASAN_OPTIONS="suppressions=asan_suppressions.txt"
export UBSAN_OPTIONS="suppressions=ubsan_suppressions.txt"
```

#### Memory Limits
```bash
# Increase memory for Docker containers
docker run --memory=4g --memory-swap=8g ...
```

#### Symbolization
```bash
# Ensure debug symbols are available
cmake .. -DCMAKE_BUILD_TYPE=Debug -g3
```

### Performance Demo Impact
The function pointer performance optimization works seamlessly with sanitizers:
- ✅ Zero-branching pattern preserved
- ✅ Runtime switching still works
- ✅ Performance overhead from sanitizers, not from optimization
- ✅ Production builds maintain full performance

## Integration with Seven Framework

The sanitizers are fully integrated with our architecture:

### Function Pointer Optimization
- Sanitizers detect issues in both fast and traced implementations
- Runtime switching between modes is safely monitored
- No performance penalty in production builds

### OpenTelemetry Integration
- Sanitizers work with both real OpenTelemetry and stub implementations
- Memory issues in tracing code are detected
- Thread safety in distributed tracing is verified

### NATS Messaging
- Message handling memory safety verified
- Concurrent message processing monitored by TSan
- Buffer management checked by ASan

## Example Output

### AddressSanitizer Detection
```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000010
READ of size 4 at 0x602000000010 thread T0
    #0 0x45b87c in ServiceHost::publish_broadcast_traced()
    #1 0x45c123 in main()
```

### UBSan Detection
```
runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
    #0 0x45b87c in calculate_performance_metrics()
```

### ThreadSanitizer Detection
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7b0400000000 by thread T1:
    #0 ServiceHost::publish_broadcast_traced()
  Previous read of size 8 at 0x7b0400000000 by main thread:
    #0 ServiceHost::enable_tracing()
```

This comprehensive sanitizer setup helps ensure the Seven framework maintains high reliability and performance! 🚀
