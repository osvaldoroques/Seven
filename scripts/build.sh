#!/usr/bin/env bash

set -e

BUILD_DIR="build"

echo "🔨 Creating build directory..."
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

echo "🔨 Generating build system with CMake..."
cmake .. -GNinja

echo "🔨 Building..."
ninja

echo "✅ Build complete."
