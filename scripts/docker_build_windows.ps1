# PowerShell Script for Building Seven Microservices with Docker
# Enhanced version with error handling and logging

param(
    [switch]$NoCache,
    [switch]$StartServices,
    [switch]$Rebuild,
    [string]$LogLevel = "INFO"
)

# Set error action preference
$ErrorActionPreference = "Stop"

# Function to write colored output
function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

# Function to log with timestamp
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $colorMap = @{
        "INFO" = "White"
        "SUCCESS" = "Green"
        "WARNING" = "Yellow"
        "ERROR" = "Red"
        "DEBUG" = "Cyan"
    }
    Write-ColorOutput "[$timestamp] [$Level] $Message" $colorMap[$Level]
}

# Main script
try {
    Write-ColorOutput "Seven Microservices - Docker Build Script (PowerShell)" "Cyan"
    Write-ColorOutput ("=" * 60) "DarkGray"
    
    # Check Docker availability
    Write-Log "Checking Docker availability..." "DEBUG"
    
    try {
        $dockerVersion = docker --version
        Write-Log "Docker is available: $dockerVersion" "SUCCESS"
    }
    catch {
        Write-Log "Docker is not installed or not available in PATH" "ERROR"
        Write-Log "Please install Docker Desktop for Windows and ensure it's running" "ERROR"
        exit 1
    }

    # Determine Docker Compose command
    Write-Log "Checking Docker Compose availability..." "DEBUG"
    
    $composeCmd = $null
    try {
        docker compose version | Out-Null
        $composeCmd = "docker compose"
        $composeVersion = docker compose version
        Write-Log "Using Docker Compose Plugin: $composeVersion" "SUCCESS"
    }
    catch {
        try {
            docker-compose --version | Out-Null
            $composeCmd = "docker-compose"
            $composeVersion = docker-compose --version
            Write-Log "Using Docker Compose Standalone: $composeVersion" "SUCCESS"
        }
        catch {
            Write-Log "Neither 'docker compose' nor 'docker-compose' is available" "ERROR"
            Write-Log "Please install Docker Compose" "ERROR"
            exit 1
        }
    }

    Write-ColorOutput "" 
    Write-Log "Starting Docker build process..." "INFO"

    # Stop existing containers if rebuild requested
    if ($Rebuild) {
        Write-Log "Stopping and removing existing containers..." "INFO"
        if ($composeCmd -eq "docker compose") {
            & docker compose down
        } else {
            & docker-compose down
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Warning: Failed to stop some containers" "WARNING"
        }
    }

    # Build Docker images
    if ($NoCache) {
        Write-Log "Building Docker images (no cache)..." "INFO"
        if ($composeCmd -eq "docker compose") {
            & docker compose build --no-cache
        } else {
            & docker-compose build --no-cache
        }
    } else {
        Write-Log "Building Docker images..." "INFO"
        if ($composeCmd -eq "docker compose") {
            & docker compose build
        } else {
            & docker-compose build
        }
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Log "Docker build failed with exit code $LASTEXITCODE" "ERROR"
        exit 1
    }

    Write-Log "Docker images built successfully!" "SUCCESS"

    # Start services if requested
    if ($StartServices) {
        Write-ColorOutput ""
        Write-Log "Starting Seven Microservices..." "INFO"
        
        if ($composeCmd -eq "docker compose") {
            & docker compose up -d
        } else {
            & docker-compose up -d
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-Log "Services started successfully!" "SUCCESS"
            
            Write-ColorOutput ""
            Write-ColorOutput "Container Status:" "Cyan"
            if ($composeCmd -eq "docker compose") {
                & docker compose ps
            } else {
                & docker-compose ps
            }
            
            Write-ColorOutput ""
            Write-ColorOutput "Service URLs:" "Cyan"
            Write-ColorOutput "  - NATS Server: nats://localhost:4222" "White"
            Write-ColorOutput "  - NATS Monitoring: http://localhost:8222" "White"
            
            Write-ColorOutput ""
            Write-ColorOutput "Useful Commands:" "Cyan"
            Write-ColorOutput "  - View logs: $composeCmd logs -f" "White"
            Write-ColorOutput "  - Stop services: $composeCmd down" "White"
            Write-ColorOutput "  - Restart: $composeCmd restart" "White"
        } else {
            Write-Log "Failed to start services" "ERROR"
            exit 1
        }
    } else {
        Write-ColorOutput ""
        Write-Log "Build completed! Use -StartServices to start services automatically" "INFO"
        Write-ColorOutput "To start services manually: $composeCmd up -d" "Yellow"
    }

    Write-ColorOutput ""
    Write-Log "Docker build process completed successfully!" "SUCCESS"

} catch {
    Write-Log "An unexpected error occurred: $($_.Exception.Message)" "ERROR"
    Write-Log "Stack trace: $($_.ScriptStackTrace)" "DEBUG"
    exit 1
}

# Usage examples in comments
<#
.SYNOPSIS
    Build Seven Microservices Docker containers on Windows

.DESCRIPTION
    This PowerShell script builds Docker containers for the Seven Microservices framework.
    It includes error handling, logging, and various options for different build scenarios.

.PARAMETER NoCache
    Build Docker images without using cache (clean build)

.PARAMETER StartServices
    Automatically start services after building

.PARAMETER Rebuild
    Stop existing containers before building

.PARAMETER LogLevel
    Set logging level (INFO, DEBUG, WARNING, ERROR)

.EXAMPLE
    .\docker_build_windows.ps1
    Basic build without starting services

.EXAMPLE
    .\docker_build_windows.ps1 -NoCache -StartServices
    Clean build and start services

.EXAMPLE
    .\docker_build_windows.ps1 -Rebuild -StartServices
    Stop existing containers, rebuild, and start services

.NOTES
    Requires Docker Desktop for Windows
    Supports both Docker Compose plugin and standalone versions
#>
