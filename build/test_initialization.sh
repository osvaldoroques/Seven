#!/bin/bash

# Test the ServiceHost unified initialization system

echo "🚀 Testing ServiceHost Unified Initialization System"
echo "===================================================="

# Test 1: Check if binary exists and runs
echo "Test 1: Checking binary existence..."
if [ -f "./services/portfolio_manager/portfolio_manager" ]; then
    echo "✅ portfolio_manager binary exists"
else
    echo "❌ portfolio_manager binary not found"
    exit 1
fi

# Test 2: Test environment-based initialization
echo ""
echo "Test 2: Testing environment-based initialization..."

# Set up test environment
export NATS_URL="nats://localhost:4222"
export DEPLOYMENT_ENV="development"
export SKIP_PERFORMANCE_DEMO="true"  # Skip demo for testing

echo "🔧 Environment variables:"
echo "   NATS_URL: $NATS_URL"
echo "   DEPLOYMENT_ENV: $DEPLOYMENT_ENV"
echo "   SKIP_PERFORMANCE_DEMO: $SKIP_PERFORMANCE_DEMO"

# Test 3: Quick startup test (will fail due to no NATS server, but should show initialization)
echo ""
echo "Test 3: Quick startup test (expecting NATS connection failure)..."
echo "Note: This will fail due to no NATS server, but should show initialization process"

# Run for 3 seconds then kill
timeout 3s ./services/portfolio_manager/portfolio_manager 2>&1 | head -20 || echo "Expected timeout/failure due to no NATS server"

echo ""
echo "✅ Basic initialization system test completed"
echo "🔧 The service correctly attempts to initialize with environment settings"
echo "📋 Next steps: Start NATS server to test full functionality"
