# 快速重编译脚本 - 仅编译更改的代码
# 不会重新配置CMake或重新编译依赖库

param(
    [string]$BuildType = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $RepoRoot "build"
$BuildScript = Join-Path $ScriptDir "build.ps1"

$generateStamp = Join-Path $BuildDir "CMakeFiles\generate.stamp.list"
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"

if (-not (Test-Path $BuildDir) -or -not (Test-Path $cacheFile) -or -not (Test-Path $generateStamp)) {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  Water Test System - Quick Rebuild" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "" 
    Write-Host "Build directory is incomplete; falling back to full CMake reconfigure..." -ForegroundColor Yellow
    & $BuildScript -BuildType $BuildType
    exit $LASTEXITCODE
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Water Test System - Quick Rebuild" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path "build")) {
    Write-Host "Error: Build directory not found. Run build.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host ""
Write-Host "Compiling changed files only..." -ForegroundColor Cyan

cmake --build build --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Executable: build/bin/$BuildType/WaterTestSystem.exe" -ForegroundColor Yellow
