@echo off
REM Windows Batch Script for Building Seven Microservices with Docker
REM Created for Windows environments

setlocal EnableDelayedExpansion

echo 🚀 Seven Microservices - Docker Build Script for Windows
echo.

REM Check if Docker is available
docker --version >nul 2>&1
if !errorlevel! neq 0 (
    echo ❌ Docker is not installed or not available in PATH
    echo 💡 Please install Docker Desktop for Windows
    echo 💡 Make sure Docker Desktop is running
    pause
    exit /b 1
)

echo ✅ Docker is available
docker --version

REM Check Docker Compose availability
docker compose version >nul 2>&1
if !errorlevel! equ 0 (
    set COMPOSE_CMD=docker compose
    echo ✅ Using Docker Compose Plugin: docker compose
) else (
    docker-compose --version >nul 2>&1
    if !errorlevel! equ 0 (
        set COMPOSE_CMD=docker-compose
        echo ✅ Using Docker Compose Standalone: docker-compose
    ) else (
        echo ❌ Neither 'docker compose' nor 'docker-compose' is available
        echo 💡 Please install Docker Compose
        pause
        exit /b 1
    )
)

echo.
echo 🐳 Building Seven Microservices Docker Images...
echo.

REM Stop and remove existing containers
echo 📋 Stopping existing containers...
%COMPOSE_CMD% down

REM Build Docker images with no cache for fresh build
echo 📋 Building Docker images (no cache)...
%COMPOSE_CMD% build --no-cache

if !errorlevel! neq 0 (
    echo ❌ Docker build failed
    pause
    exit /b 1
)

echo.
echo ✅ Docker images built successfully!
echo.

REM Ask user if they want to start the services
echo 🤔 Do you want to start the services now? (Y/N)
set /p choice="Enter your choice: "

if /i "!choice!"=="Y" goto start_services
if /i "!choice!"=="YES" goto start_services
goto end

:start_services
echo.
echo 🚀 Starting Seven Microservices...
%COMPOSE_CMD% up -d

if !errorlevel! equ 0 (
    echo.
    echo ✅ Services started successfully!
    echo.
    echo 📊 Container Status:
    %COMPOSE_CMD% ps
    echo.
    echo 🌐 Service URLs:
    echo   • NATS Server: nats://localhost:4222
    echo   • NATS Monitoring: http://localhost:8222
    echo.
    echo 📝 Useful Commands:
    echo   • View logs: %COMPOSE_CMD% logs -f
    echo   • Stop services: %COMPOSE_CMD% down
    echo   • Restart: %COMPOSE_CMD% restart
) else (
    echo ❌ Failed to start services
    pause
    exit /b 1
)

:end
echo.
echo 🎉 Build process completed!
echo 💡 You can manually start services later with: %COMPOSE_CMD% up -d
pause
