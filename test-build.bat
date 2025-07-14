@echo off
REM Test script for Windows to build the enhanced observability stack
echo ========================================
echo Testing OpenTelemetry Enhanced Build
echo ========================================

echo Step 1: Building portfolio_manager with OpenTelemetry...
docker compose build portfolio_manager
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Portfolio manager build failed
    pause
    exit /b 1
)

echo ✅ Portfolio manager build successful!
echo.

echo Step 2: Building complete observability stack...
docker compose build
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Complete stack build failed
    pause
    exit /b 1
)

echo ✅ Complete stack build successful!
echo.

echo Step 3: Starting observability stack...
docker compose up -d
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Stack startup failed
    pause
    exit /b 1
)

echo ✅ Observability stack started!
echo.

echo Step 4: Checking service status...
docker compose ps

echo.
echo ========================================
echo 🎉 Build Test Complete!
echo ========================================
echo Services available:
echo - Portfolio Manager: http://localhost:8080
echo - Jaeger UI: http://localhost:16686
echo - OTEL Collector: http://localhost:4317 (gRPC), http://localhost:4318 (HTTP)
echo - NATS: http://localhost:4222
echo.
echo To view logs: docker compose logs -f portfolio_manager
echo To stop stack: docker compose down
echo.
pause
