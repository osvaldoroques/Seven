@echo off
REM Quick test script for OpenTelemetry CMake fix
echo ========================================
echo Testing OpenTelemetry CMake Fix
echo ========================================

echo Step 1: Building portfolio_manager with fixed OpenTelemetry detection...
docker compose build portfolio_manager --no-cache
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Build still failing - checking logs...
    docker compose logs portfolio_manager
    pause
    exit /b 1
)

echo ✅ Build successful with OpenTelemetry fix!
echo.

echo Step 2: Quick container verification...
docker compose up -d portfolio_manager otel-collector jaeger nats
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Container startup failed
    pause
    exit /b 1
)

echo ✅ Containers started successfully!
echo.

echo Step 3: Checking container logs for OpenTelemetry...
timeout /t 5 /nobreak >nul
docker compose logs portfolio_manager | findstr /i "opentelemetry trace"

echo.
echo ========================================
echo 🎉 OpenTelemetry Fix Test Complete!
echo ========================================
echo.
echo To view full logs: docker compose logs -f portfolio_manager
echo To access Jaeger UI: http://localhost:16686
echo To stop: docker compose down
echo.
pause
