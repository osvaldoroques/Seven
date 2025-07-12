#!/bin/bash

# Clean Docker build test script
echo "🧹 Cleaning Docker environment..."

# Stop and remove containers
docker compose down --remove-orphans 2>/dev/null || true

# Remove images if they exist
docker rmi seven-portfolio_manager 2>/dev/null || true
docker rmi seven-python_client 2>/dev/null || true

# Prune build cache
docker builder prune -f

echo "✅ Environment cleaned"
echo "🔨 Starting fresh build..."

# Build with verbose output
docker compose build --no-cache --progress=plain

echo "🎯 Build completed"
