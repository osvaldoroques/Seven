@echo off
echo 🪟 Windows Docker Build for Seven Project
echo ==========================================

REM Check if Docker is available
docker version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ Docker is not running. Please start Docker Desktop.
    exit /b 1
)
echo ✅ Docker is running

REM Clean build directory
if exist build (
    echo 🧹 Cleaning existing build directory...
    rmdir /s /q build 2>nul
)

REM Build with Windows-optimized Dockerfile
echo 🚀 Building with Windows-optimized Dockerfile...
docker build -f Dockerfile.windows --target portfolio_manager -t seven-portfolio-windows .

if %errorlevel% equ 0 (
    echo ✅ Windows build successful!
    
    echo 🧪 Testing container...
    docker run --rm seven-portfolio-windows echo Container test passed
    
    if %errorlevel% equ 0 (
        echo ✅ Container test successful!
        echo 🎉 Portfolio Manager is ready to deploy!
    )
) else (
    echo ❌ Build failed. Try these debugging steps:
    echo 1. Check Docker Desktop is running
    echo 2. Try: docker system prune -f
    echo 3. Try: docker build --no-cache ...
)

echo 🏁 Build script completed
pause
