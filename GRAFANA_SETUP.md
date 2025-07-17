# Seven Grafana Integration Guide

## Overview

This setup integrates Grafana as a dedicated, standalone service in the Seven microservices Docker-Compose stack for comprehensive observability.

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Grafana       │    │  OpenTelemetry  │    │ Portfolio       │
│   Dashboard     │◄───┤   Collector     │◄───┤ Manager         │
│   (Port 3000)   │    │  (Port 8888)    │    │ (Port 9090)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────▼─────────────┐
                    │      Jaeger Tracing      │
                    │      (Port 16686)        │
                    └───────────────────────────┘
```

## Key Features

### 🔧 **Standalone Service**
- Grafana runs as an independent Docker service
- No coupling with application containers
- Independent scaling and upgrades
- Persistent data storage with Docker volumes

### 📊 **Pre-configured Dashboards**
- **ServiceHost Dashboard**: Comprehensive metrics for your ServiceHost implementation
- **System Overview**: High-level view of all services
- **Custom Prometheus Metrics**: Direct integration with your ServiceHost metrics

### 🔌 **Multiple Data Sources**
- **Prometheus**: Metrics from OpenTelemetry Collector (port 8888)
- **Direct ServiceHost**: Direct connection to Portfolio Manager metrics (port 9090)
- **Jaeger**: Distributed tracing integration

### 🚀 **Easy Setup**
- Single command startup: `./start-observability.sh`
- Automatic service health checking
- Development mode with debug logging
- Preconfigured datasources and dashboards

## Getting Started

### 1. Start the Stack
```bash
# Production mode
./start-observability.sh

# Development mode (with debug logging)
./start-observability.sh dev
```

### 2. Access Grafana
- URL: http://localhost:3000
- Username: `admin`
- Password: `admin`

### 3. View Dashboards
Navigate to:
- **Dashboards** → **Seven ServiceHost Dashboard**
- **Dashboards** → **Seven System Overview**

## ServiceHost Metrics Available

Your ServiceHost implementation exposes these metrics on port 9090:

### Counters
- `servicehost_messages_sent_total` - Total messages sent
- `servicehost_messages_received_total` - Total messages received
- `servicehost_nats_published_total` - NATS messages published
- `servicehost_nats_received_total` - NATS messages received

### Gauges
- `servicehost_cpu_usage_percent` - CPU usage percentage
- `servicehost_memory_usage_mb` - Memory usage in MB
- `servicehost_thread_pool_active_threads` - Active threads in pool
- `servicehost_thread_pool_queue_size` - Thread pool queue size
- `servicehost_cache_size` - Current cache size
- `servicehost_cache_hit_ratio` - Cache hit ratio
- `servicehost_nats_connection_status` - NATS connection status

### Histograms
- `servicehost_handler_latency` - Message handler latency distribution
- `servicehost_publish_latency` - Message publish latency distribution

## Configuration Files

### Docker Compose
- `docker-compose.yml` - Main stack configuration
- `docker-compose.override.yml` - Development overrides

### Grafana Configuration
- `config/grafana/provisioning/datasources/prometheus.yml` - Data source configuration
- `config/grafana/provisioning/dashboards/dashboards.yml` - Dashboard provisioning
- `config/grafana/dashboards/servicehost-dashboard.json` - ServiceHost dashboard
- `config/grafana/dashboards/system-overview.json` - System overview dashboard

### OpenTelemetry
- `config/otel-collector-config.yaml` - Updated with Prometheus scraping

## Customization

### Adding New Dashboards
1. Create JSON dashboard files in `config/grafana/dashboards/`
2. Restart Grafana service: `docker-compose restart grafana`
3. Dashboards will be auto-loaded

### Adding New Metrics
1. Add metrics to your ServiceHost implementation
2. Update dashboard queries to include new metrics
3. Grafana will automatically discover new metrics

### Scaling Grafana
```bash
# Scale Grafana service
docker-compose up -d --scale grafana=2

# Use load balancer for high availability
# (requires additional configuration)
```

## Troubleshooting

### Grafana Not Loading Dashboards
```bash
# Check provisioning logs
docker-compose logs grafana

# Verify file permissions
ls -la config/grafana/dashboards/
```

### Metrics Not Appearing
```bash
# Check if Portfolio Manager is exposing metrics
curl http://localhost:9090/metrics

# Check OpenTelemetry Collector
curl http://localhost:8888/metrics

# Check service connectivity
docker-compose exec grafana curl http://portfolio_manager:9090/metrics
```

### Performance Issues
```bash
# Check resource usage
docker stats

# Adjust memory limits in docker-compose.yml
# Add persistent storage for better performance
```

## Production Considerations

### Security
- Change default Grafana password
- Use environment variables for sensitive data
- Enable HTTPS with proper certificates
- Restrict network access

### Monitoring
- Monitor Grafana service health
- Set up alerts for dashboard failures
- Monitor disk space for persistent volumes

### Backup
- Backup Grafana configuration and dashboards
- Use version control for dashboard JSON files
- Regular backup of persistent volumes

## Integration with CI/CD

### Dashboard as Code
```bash
# Export dashboard
curl -X GET "http://admin:admin@localhost:3000/api/dashboards/uid/servicehost-dashboard" | jq '.dashboard' > new-dashboard.json

# Import dashboard
curl -X POST "http://admin:admin@localhost:3000/api/dashboards/db" -H "Content-Type: application/json" -d @new-dashboard.json
```

### Automated Testing
```bash
# Test dashboard queries
curl -X POST "http://admin:admin@localhost:3000/api/datasources/proxy/1/api/v1/query" -H "Content-Type: application/json" -d '{"query":"servicehost_messages_sent_total"}'
```

This setup provides a solid foundation for observability in your Seven microservices architecture with Grafana as a dedicated, scalable service.
