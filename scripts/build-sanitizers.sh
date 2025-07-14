#!/bin/bash
# Build script with AddressSanitizer and UBSan enabled

set -e

echo "🔍 Building with AddressSanitizer and UndefinedBehaviorSanitizer"
echo "=================================================="

# Create build directory
BUILD_DIR="build-sanitizers"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with sanitizers
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ASAN=ON \
    -DENABLE_UBSAN=ON \
    -DENABLE_TESTS=OFF

# Build
ninja

echo ""
echo "✅ Build complete with sanitizers enabled!"
echo "🔍 AddressSanitizer: Detects memory errors, leaks, use-after-free"
echo "🔍 UBSan: Detects undefined behavior, integer overflow, null pointer dereference"
echo ""
echo "To run with sanitizers:"
echo "  cd $BUILD_DIR"
echo "  ./services/portfolio_manager/portfolio_manager"
echo ""
echo "Environment variables for better output:"
echo "  export ASAN_OPTIONS='verbosity=1:abort_on_error=1:check_initialization_order=1'"
echo "  export UBSAN_OPTIONS='print_stacktrace=1:abort_on_error=1'"
