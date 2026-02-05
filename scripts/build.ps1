# 构建脚本 - 完整编译（首次使用）
# 会重新配置CMake并编译所有内容

param(
    [string]$BuildType = "Release"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Water Test System - Full Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查vcpkg环境变量
if (-not $env:VCPKG_ROOT) {
    Write-Host "Error: VCPKG_ROOT environment variable not set" -ForegroundColor Red
    exit 1
}

# 启用vcpkg二进制缓存
if (-not $env:VCPKG_BINARY_SOURCES) {
    $env:VCPKG_BINARY_SOURCES = "clear;files,$env:USERPROFILE\.vcpkg\bincache,readwrite"
    Write-Host "Enabled vcpkg binary cache: $env:VCPKG_BINARY_SOURCES" -ForegroundColor Green
}

Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host ""

# 配置CMake
Write-Host "Configuring CMake..." -ForegroundColor Cyan
cmake -B build `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan
cmake --build build --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "Executable: build/bin/$BuildType/WaterTestSystem.exe" -ForegroundColor Yellow
