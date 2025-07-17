#!/bin/bash

# Seven Observability Stack Startup Script
# This script starts the complete Seven microservices stack with observability

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    print_error "Docker is not running. Please start Docker Desktop and try again."
    exit 1
fi

# Check if Docker Compose is available
if ! command -v docker-compose > /dev/null 2>&1; then
    print_error "Docker Compose is not installed. Please install Docker Compose and try again."
    exit 1
fi

print_header "Starting Seven Observability Stack"

# Create necessary directories
print_status "Creating configuration directories..."
mkdir -p config/grafana/provisioning/datasources
mkdir -p config/grafana/provisioning/dashboards
mkdir -p config/grafana/dashboards
mkdir -p config/grafana/plugins

# Check if configuration files exist
if [ ! -f "config/grafana/provisioning/datasources/prometheus.yml" ]; then
    print_warning "Grafana datasource configuration missing. Please ensure all config files are in place."
fi

# Build the application
print_status "Building Seven services..."
docker-compose build

# Start the stack
print_status "Starting services..."
if [ "$1" = "dev" ]; then
    print_status "Starting in development mode with debug logging..."
    docker-compose -f docker-compose.yml -f docker-compose.override.yml up -d
else
    docker-compose up -d
fi

# Wait for services to be ready
print_status "Waiting for services to be ready..."
sleep 10

# Check service health
print_status "Checking service health..."

# Function to check if a service is responding
check_service() {
    local service_name=$1
    local url=$2
    local max_attempts=30
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        if curl -f -s "$url" > /dev/null 2>&1; then
            print_status "$service_name is ready! ✓"
            return 0
        fi
        
        if [ $attempt -eq $max_attempts ]; then
            print_warning "$service_name is not responding at $url after $max_attempts attempts"
            return 1
        fi
        
        sleep 2
        ((attempt++))
    done
}

# Check services
check_service "Grafana" "http://localhost:3000"
check_service "Jaeger UI" "http://localhost:16686"
check_service "OpenTelemetry Collector" "http://localhost:8888/metrics"

# Try to check Portfolio Manager metrics (might not be ready yet)
if check_service "Portfolio Manager Metrics" "http://localhost:9090/metrics"; then
    print_status "Portfolio Manager metrics endpoint is ready!"
else
    print_warning "Portfolio Manager metrics endpoint is not ready yet. This is normal if the service is still starting."
fi

print_header "Stack Started Successfully!"

echo -e "${GREEN}Services are running:${NC}"
echo -e "  📊 Grafana Dashboard: ${BLUE}http://localhost:3000${NC} (admin/admin)"
echo -e "  🔍 Jaeger Tracing:    ${BLUE}http://localhost:16686${NC}"
echo -e "  📈 Portfolio Manager: ${BLUE}http://localhost:8080${NC}"
echo -e "  📊 PM Metrics:        ${BLUE}http://localhost:9090/metrics${NC}"
echo -e "  🔄 OTEL Collector:    ${BLUE}http://localhost:8888/metrics${NC}"
echo -e "  📡 NATS:              ${BLUE}http://localhost:8222${NC}"

echo -e "\n${GREEN}Pre-configured Grafana dashboards:${NC}"
echo -e "  • Seven ServiceHost Dashboard"
echo -e "  • Seven System Overview"

echo -e "\n${YELLOW}To stop the stack:${NC}"
echo -e "  docker-compose down"

echo -e "\n${YELLOW}To view logs:${NC}"
echo -e "  docker-compose logs -f [service_name]"

echo -e "\n${YELLOW}To rebuild after changes:${NC}"
echo -e "  docker-compose build && docker-compose up -d"
