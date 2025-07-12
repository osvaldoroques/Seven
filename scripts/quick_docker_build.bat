@echo off
REM Quick Docker Build Script for Seven Microservices
REM Simple one-click build and run

echo 🚀 Seven Microservices - Quick Docker Build
echo.

REM Check Docker
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ Docker not found. Please install Docker Desktop.
    pause
    exit /b 1
)

REM Use Docker Compose
set COMPOSE_CMD=docker compose
docker compose version >nul 2>&1
if %errorlevel% neq 0 (
    set COMPOSE_CMD=docker-compose
)

echo 🐳 Building and starting services...
%COMPOSE_CMD% down
%COMPOSE_CMD% build
%COMPOSE_CMD% up -d

if %errorlevel% equ 0 (
    echo.
    echo ✅ Services running!
    echo 🌐 NATS Server: nats://localhost:4222
    echo 🌐 NATS Monitor: http://localhost:8222
    echo.
    echo Press any key to view container status...
    pause >nul
    %COMPOSE_CMD% ps
) else (
    echo ❌ Build failed!
)

echo.
pause
