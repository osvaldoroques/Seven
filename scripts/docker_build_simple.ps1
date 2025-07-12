param(
    [switch]$NoCache,
    [switch]$StartServices
)

Write-Host "Seven Microservices - Docker Build Script" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Gray

# Check Docker
try {
    docker --version | Out-Null
    Write-Host "[OK] Docker found" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Docker not found. Please install Docker Desktop." -ForegroundColor Red
    exit 1
}

# Determine compose command
$composeCmd = "docker compose"
try {
    docker compose version | Out-Null
} catch {
    $composeCmd = "docker-compose"
    Write-Host "[INFO] Using docker-compose (legacy)" -ForegroundColor Yellow
}

# Build parameters
$buildArgs = @()
if ($NoCache) {
    $buildArgs += "--no-cache"
}

Write-Host "[INFO] Building Docker images..." -ForegroundColor Cyan

# Stop existing containers
if ($composeCmd -eq "docker compose") {
    & docker compose down
} else {
    & docker-compose down
}

# Build
if ($NoCache) {
    if ($composeCmd -eq "docker compose") {
        & docker compose build --no-cache
    } else {
        & docker-compose build --no-cache
    }
} else {
    if ($composeCmd -eq "docker compose") {
        & docker compose build
    } else {
        & docker-compose build
    }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[SUCCESS] Build completed!" -ForegroundColor Green

# Start services if requested
if ($StartServices) {
    Write-Host "[INFO] Starting services..." -ForegroundColor Cyan
    if ($composeCmd -eq "docker compose") {
        & docker compose up -d
    } else {
        & docker-compose up -d
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[SUCCESS] Services started!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Service URLs:" -ForegroundColor Cyan
        Write-Host "  NATS Server: nats://localhost:4222" -ForegroundColor White
        Write-Host "  NATS Monitor: http://localhost:8222" -ForegroundColor White
    }
} else {
    Write-Host ""
    Write-Host "To start services: $composeCmd up -d" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
