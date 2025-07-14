#!/bin/bash
# Build script with ThreadSanitizer enabled (detects data races)

set -e

echo "🧵 Building with ThreadSanitizer"
echo "================================"

# Create build directory
BUILD_DIR="build-tsan"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with ThreadSanitizer
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TSAN=ON \
    -DENABLE_TESTS=OFF

# Build
ninja

echo ""
echo "✅ Build complete with ThreadSanitizer enabled!"
echo "🧵 ThreadSanitizer: Detects data races and thread safety issues"
echo ""
echo "⚠️  Note: TSan cannot be used with ASan/MSan simultaneously"
echo ""
echo "To run with TSan:"
echo "  cd $BUILD_DIR"
echo "  ./services/portfolio_manager/portfolio_manager"
echo ""
echo "Environment variables for better output:"
echo "  export TSAN_OPTIONS='verbosity=1:abort_on_error=1:halt_on_error=1'"
