#!/usr/bin/env bash

set -e

BUILD_DIR="build"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "❌ Build directory not found. Run scripts/build.sh first."
    exit 1
fi

cd ${BUILD_DIR}

echo "🧪 Running tests..."
ctest --output-on-failure

echo "✅ All tests completed."
