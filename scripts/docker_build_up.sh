#!/usr/bin/env bash

set -e

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo "❌ Docker is not installed or not available in PATH"
    echo "� If you're in a dev container, you might need to use Docker from the host system"
    exit 1
fi

# Function to check which docker compose command works
check_docker_compose() {
    if docker compose version &> /dev/null; then
        echo "docker compose"
    elif command -v docker-compose &> /dev/null; then
        echo "docker-compose"
    else
        echo "❌ Neither 'docker compose' nor 'docker-compose' is available"
        echo "💡 Please install Docker Compose or use 'docker compose' (newer syntax)"
        exit 1
    fi
}

COMPOSE_CMD=$(check_docker_compose)

echo "🐳 Building Docker images with $COMPOSE_CMD..."
$COMPOSE_CMD build

echo "🐳 Starting containers with $COMPOSE_CMD..."
$COMPOSE_CMD up
