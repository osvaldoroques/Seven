#!/usr/bin/env bash

echo "🐳 Stopping and removing previous containers..."
docker-compose down

echo "🐳 Building Docker images with docker-compose..."
docker-compose build --no-cache

echo "🐳 Starting containers with docker-compose..."
docker-compose up
