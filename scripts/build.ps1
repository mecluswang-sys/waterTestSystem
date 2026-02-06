# 构建脚本 - 完整编译（首次使用）
# 会重新配置CMake并编译所有内容

param(
    [string]$BuildType = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $RepoRoot "build"

$RepoVcpkgRoot = Join-Path $RepoRoot "vcpkg_installed\vcpkg"

if ($env:VCPKG_ROOT -and -not (Test-Path $env:VCPKG_ROOT)) {
    Write-Host "Warning: VCPKG_ROOT is set but does not exist: $env:VCPKG_ROOT" -ForegroundColor Yellow
    Write-Host "Falling back to repo vcpkg: $RepoVcpkgRoot" -ForegroundColor Yellow
    $env:VCPKG_ROOT = ""
}

if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = $RepoVcpkgRoot
}

$VcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $VcpkgToolchain)) {
    Write-Host "$VcpkgToolchain not found" -ForegroundColor Red
    exit 1
}

Write-Host "Using VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor DarkGray

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Water Test System - Full Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 启用vcpkg二进制缓存
if (-not $env:VCPKG_BINARY_SOURCES) {
    $env:VCPKG_BINARY_SOURCES = "clear;files,$env:USERPROFILE\.vcpkg\bincache,readwrite"
    Write-Host "Enabled vcpkg binary cache: $env:VCPKG_BINARY_SOURCES" -ForegroundColor Green
}

Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host ""

# 作为“完整编译/首次使用”脚本：清理旧的 CMake 缓存，避免生成器/工具链无法切换
if (Test-Path $BuildDir) {
    $CacheFile = Join-Path $BuildDir "CMakeCache.txt"
    $CacheDir = Join-Path $BuildDir "CMakeFiles"
    if (Test-Path $CacheFile) { Remove-Item $CacheFile -Force }
    if (Test-Path $CacheDir) { Remove-Item $CacheDir -Recurse -Force }
} else {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# 优先使用 Visual Studio 生成器（本机已安装 Build Tools 时可用）
$CMakeGenerator = $null
$CMakeArch = $null
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInstall) {
        $CMakeGenerator = "Visual Studio 17 2022"
        $CMakeArch = "x64"
    }
}

# 配置CMake
Write-Host "Configuring CMake..." -ForegroundColor Cyan
if ($CMakeGenerator) {
    Write-Host "CMake Generator: $CMakeGenerator ($CMakeArch)" -ForegroundColor DarkGray
    cmake -S "$RepoRoot" -B "$BuildDir" -G "$CMakeGenerator" -A "$CMakeArch" `
        -DCMAKE_BUILD_TYPE=$BuildType `
        -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain"
} else {
    cmake -S "$RepoRoot" -B "$BuildDir" `
        -DCMAKE_BUILD_TYPE=$BuildType `
        -DCMAKE_TOOLCHAIN_FILE="$VcpkgToolchain"
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan
cmake --build "$BuildDir" --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build completed successfully!" -ForegroundColor Green
$ExePath = Join-Path $BuildDir "bin\$BuildType\WaterTestSystem.exe"
Write-Host "Executable: $ExePath" -ForegroundColor Yellow
