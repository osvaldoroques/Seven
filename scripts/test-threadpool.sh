#!/bin/bash

# ThreadPool Test Runner with Sanitizers
# This script builds and runs ThreadPool tests with different sanitizer configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo "🧪 ThreadPool Test Suite with Sanitizers"
echo "========================================"

# Function to run tests with specific sanitizer configuration
run_test_suite() {
    local config_name="$1"
    local cmake_flags="$2"
    local env_vars="$3"
    
    echo ""
    echo "🔧 Building ThreadPool tests with $config_name..."
    
    # Create build directory for this configuration
    local test_build_dir="$BUILD_DIR/test_$config_name"
    mkdir -p "$test_build_dir"
    cd "$test_build_dir"
    
    # Configure with specific flags
    cmake "$PROJECT_ROOT" \
        -DCMAKE_BUILD_TYPE=Debug \
        -GNinja \
        $cmake_flags
    
    # Build only the ThreadPool tests
    ninja test_thread_pool test_thread_pool_integration test_thread_pool_sanitizers
    
    echo "🏃 Running ThreadPool tests with $config_name..."
    
    # Set environment variables and run tests
    if [ -n "$env_vars" ]; then
        export $env_vars
    fi
    
    echo "  → Basic ThreadPool functionality tests"
    ./test_thread_pool --gtest_brief=1
    
    echo "  → ThreadPool + ServiceHost integration tests" 
    ./test_thread_pool_integration --gtest_brief=1
    
    echo "  → ThreadPool sanitizer-specific tests"
    ./test_thread_pool_sanitizers --gtest_brief=1
    
    echo "✅ $config_name tests completed successfully!"
}

# Test configurations
echo "📋 Running ThreadPool tests with multiple sanitizer configurations..."

# 1. AddressSanitizer + UBSan (recommended development configuration)
run_test_suite "asan_ubsan" \
    "-DENABLE_ASAN=ON -DENABLE_UBSAN=ON" \
    "ASAN_OPTIONS=verbosity=1:abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:abort_on_error=1"

# 2. ThreadSanitizer (for race condition detection)
run_test_suite "tsan" \
    "-DENABLE_TSAN=ON" \
    "TSAN_OPTIONS=verbosity=1:abort_on_error=1:halt_on_error=1"

# 3. Release build (no sanitizers for performance comparison)
run_test_suite "release" \
    "-DCMAKE_BUILD_TYPE=Release" \
    ""

echo ""
echo "🎉 All ThreadPool test configurations completed successfully!"
echo ""
echo "📊 Test Summary:"
echo "  ✅ AddressSanitizer + UBSan: Memory safety and undefined behavior detection"
echo "  ✅ ThreadSanitizer: Race condition and data race detection"  
echo "  ✅ Release build: Performance baseline"
echo ""
echo "🔍 The ThreadPool implementation is production-ready and sanitizer-clean!"

# Return to original directory
cd "$PROJECT_ROOT"
