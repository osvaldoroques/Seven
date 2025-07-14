#!/bin/bash
# Build script for production (no sanitizers, optimized)

set -e

echo "🚀 Building for Production (Release mode, no sanitizers)"
echo "======================================================="

# Create build directory
BUILD_DIR="build-release"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure for production
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ASAN=OFF \
    -DENABLE_UBSAN=OFF \
    -DENABLE_TSAN=OFF \
    -DENABLE_MSAN=OFF \
    -DENABLE_TESTS=OFF

# Build
ninja

echo ""
echo "✅ Production build complete!"
echo "🚀 Optimized release build without sanitizers"
echo "📦 Ready for deployment"
echo ""
echo "To run production build:"
echo "  cd $BUILD_DIR"
echo "  ./services/portfolio_manager/portfolio_manager"
