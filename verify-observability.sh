#!/bin/bash
# Verification script to test OpenTelemetry observability features

echo "========================================="
echo "OpenTelemetry Observability Verification"
echo "========================================="

echo "🔍 Checking service health..."
echo

# Check container status
echo "Container Status:"
docker compose ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"
echo

# Check portfolio manager logs for structured logs with trace IDs
echo "📋 Recent Portfolio Manager Logs (looking for trace_id):"
docker compose logs --tail=10 portfolio_manager | grep -E "(trace_id|span_id|INFO|ERROR|WARN)" || echo "No structured logs found yet"
echo

# Check OTEL Collector logs
echo "📡 OTEL Collector Status:"
docker compose logs --tail=5 otel-collector | grep -E "(receiver|exporter|pipeline)" || echo "Collector starting up..."
echo

# Test service endpoints
echo "🌐 Testing Service Endpoints:"

# Test portfolio manager health
if curl -s -f http://localhost:8080/health >/dev/null 2>&1; then
    echo "✅ Portfolio Manager: http://localhost:8080 - HEALTHY"
else
    echo "❌ Portfolio Manager: http://localhost:8080 - NOT RESPONDING"
fi

# Test Jaeger UI
if curl -s -f http://localhost:16686 >/dev/null 2>&1; then
    echo "✅ Jaeger UI: http://localhost:16686 - AVAILABLE"
else
    echo "❌ Jaeger UI: http://localhost:16686 - NOT AVAILABLE"
fi

# Test OTEL Collector health
if curl -s -f http://localhost:8888/metrics >/dev/null 2>&1; then
    echo "✅ OTEL Collector: http://localhost:8888 - HEALTHY"
else
    echo "❌ OTEL Collector: http://localhost:8888 - NOT RESPONDING"
fi

echo
echo "🔗 Next Steps:"
echo "1. Open Jaeger UI: http://localhost:16686"
echo "2. Look for 'portfolio_manager' service in the dropdown"
echo "3. View traces with correlation IDs matching log entries"
echo "4. Check structured logs: docker compose logs -f portfolio_manager"
echo
echo "To generate test traces, trigger portfolio manager operations"
echo "========================================="
