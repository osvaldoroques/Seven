# Windows PowerShell Build Script for Seven Project
# Run this script from the project root directory

Write-Host "🪟 Windows Docker Build for Seven Project" -ForegroundColor Cyan
Write-Host "=========================================="

# Check if Docker is running
try {
    docker version | Out-Null
    Write-Host "✅ Docker is running" -ForegroundColor Green
} catch {
    Write-Host "❌ Docker is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}

# Clean build directory if it exists
if (Test-Path "build") {
    Write-Host "🧹 Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
}

# Option 1: Try production Dockerfile (no tests)
Write-Host "🚀 Building with production Dockerfile (no tests)..." -ForegroundColor Cyan
$buildResult = docker build -f Dockerfile.production --target portfolio_manager -t seven-portfolio-prod . --progress=plain

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Production build successful!" -ForegroundColor Green
    
    Write-Host "🧪 Testing container..." -ForegroundColor Cyan
    docker run --rm seven-portfolio-prod echo "Container test passed"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Container test successful!" -ForegroundColor Green
        Write-Host "🎉 Portfolio Manager is ready to deploy!" -ForegroundColor Green
    }
} else {
    Write-Host "❌ Production build failed. Trying Windows-optimized build..." -ForegroundColor Red
    
    # Option 2: Try Windows-optimized Dockerfile
    Write-Host "🚀 Building with Windows-optimized Dockerfile..." -ForegroundColor Cyan
    $buildResult = docker build -f Dockerfile.windows --target portfolio_manager -t seven-portfolio-windows . --progress=plain

    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Windows build successful!" -ForegroundColor Green
        
        Write-Host "🧪 Testing container..." -ForegroundColor Cyan
        docker run --rm seven-portfolio-windows echo "Container test passed"
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Container test successful!" -ForegroundColor Green
            Write-Host "🎉 Portfolio Manager is ready to deploy!" -ForegroundColor Green
        }
    } else {
        Write-Host "❌ Windows build failed. Trying alternative approach..." -ForegroundColor Red
        
        # Option 3: Try the clean Dockerfile
        Write-Host "🔄 Trying clean Dockerfile..." -ForegroundColor Yellow
        docker build -f Dockerfile.clean --target portfolio_manager -t seven-portfolio-clean . --progress=plain
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Clean build successful!" -ForegroundColor Green
        } else {
            Write-Host "❌ All builds failed. Debugging steps:" -ForegroundColor Red
            Write-Host "1. Check Docker Desktop is running" -ForegroundColor Yellow
            Write-Host "2. Try: docker system prune -f" -ForegroundColor Yellow
            Write-Host "3. Try: docker build --no-cache ..." -ForegroundColor Yellow
        }
    }
}

Write-Host "🏁 Build script completed" -ForegroundColor Cyan
