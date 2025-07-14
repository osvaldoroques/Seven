#!/bin/bash

echo "🔍 OpenTelemetry Distributed Tracing - Seven Microservices"
echo "========================================================="

# Check if services are running
if ! docker compose ps | grep -q "running"; then
    echo "❌ Services not running. Starting them now..."
    docker compose up -d
    echo "⏱️  Waiting for services to start..."
    sleep 30
fi

echo ""
echo "📊 Service URLs:"
echo "  🟢 Jaeger UI: http://localhost:16686"
echo "  🟢 NATS Server: nats://localhost:4222"  
echo "  🟢 NATS Monitor: http://localhost:8222"
echo "  🟢 OTEL Collector Metrics: http://localhost:8888/metrics"

echo ""
echo "🔍 Checking service health..."

# Check Jaeger
if curl -s http://localhost:16686/api/services >/dev/null 2>&1; then
    echo "  ✅ Jaeger UI is accessible"
else
    echo "  ❌ Jaeger UI not accessible"
fi

# Check OTEL Collector
if curl -s http://localhost:8888/metrics >/dev/null 2>&1; then
    echo "  ✅ OTEL Collector is running"
else
    echo "  ❌ OTEL Collector not accessible"
fi

# Check if containers are running
echo ""
echo "📦 Container Status:"
docker compose ps

echo ""
echo "📝 Recent Portfolio Manager Logs:"
docker compose logs --tail=10 portfolio_manager

echo ""
echo "🎯 To view distributed traces:"
echo "  1. Open: http://localhost:16686"
echo "  2. Select service: portfolio_manager or python_client"
echo "  3. Click 'Find Traces' to see request flows"
echo ""
echo "🚀 To generate traces, trigger some requests:"
echo "  • Health checks from python_client will auto-generate traces"
echo "  • Portfolio requests will show database and calculation spans"

# Open Jaeger UI if on supported platform
if command -v xdg-open >/dev/null 2>&1; then
    echo ""
    echo "🌐 Opening Jaeger UI in browser..."
    xdg-open http://localhost:16686
elif command -v open >/dev/null 2>&1; then
    echo ""
    echo "🌐 Opening Jaeger UI in browser..."
    open http://localhost:16686
else
    echo ""
    echo "🌐 Please open http://localhost:16686 in your browser"
fi
