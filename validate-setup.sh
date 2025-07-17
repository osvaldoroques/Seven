#!/bin/bash

# Grafana Observability Stack Validation
# This script validates the Grafana setup without starting Docker

# Change to the Seven workspace directory
cd /workspaces/Seven

echo "🔍 Validating Grafana Observability Stack Configuration..."
echo "📂 Working directory: $(pwd)"
echo ""

# Check if all required files exist
echo "📁 Checking configuration files..."

files_to_check=(
    "docker-compose.yml"
    "config/grafana/provisioning/datasources/prometheus.yml"
    "config/grafana/provisioning/dashboards/dashboards.yml"
    "config/grafana/dashboards/servicehost-dashboard.json"
    "config/grafana/dashboards/system-overview.json"
    "config/otel-collector-config.yaml"
    "start-observability.sh"
)

all_files_exist=true
for file in "${files_to_check[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file exists"
    else
        echo "❌ $file missing"
        all_files_exist=false
    fi
done

echo ""
echo "📋 Configuration Summary:"
echo "   • Grafana will run on port 3000"
echo "   • OpenTelemetry Collector on port 8888"
echo "   • Portfolio Manager metrics on port 9090"
echo "   • Jaeger UI on port 16686"
echo "   • NATS server on port 4222"
echo ""

if [ "$all_files_exist" = true ]; then
    echo "✅ All configuration files are present!"
    echo ""
    echo "🚀 To start the observability stack when Docker is available:"
    echo "   1. Run: bash start-observability.sh"
    echo "   2. Or manually: docker-compose up -d"
    echo ""
    echo "📊 Once running, access:"
    echo "   • Grafana: http://localhost:3000 (admin/admin)"
    echo "   • Jaeger: http://localhost:16686"
    echo "   • Prometheus metrics: http://localhost:9090/metrics"
    echo ""
    echo "🎯 The setup includes:"
    echo "   • Dedicated Grafana service with persistence"
    echo "   • Auto-provisioned datasources (Prometheus + Jaeger)"
    echo "   • Pre-configured dashboards for ServiceHost metrics"
    echo "   • Complete observability stack isolation"
    echo "   • Independent service scaling capabilities"
    echo ""
else
    echo "❌ Some configuration files are missing. Please check the setup."
fi

echo "📖 For detailed setup information, see: GRAFANA_SETUP.md"
