#!/bin/sh
cd /workspaces/Seven

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure CMake with path to locally installed libraries
cmake .. -DCMAKE_PREFIX_PATH=$HOME/local

# Build the portfolio_manager target
cmake --build . --target portfolio_manager
