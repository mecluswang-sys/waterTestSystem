# vcpkg二进制缓存设置脚本

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  vcpkg Binary Cache Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$cacheDir = "$env:USERPROFILE\.vcpkg\bincache"

# 创建缓存目录
if (-not (Test-Path $cacheDir)) {
    New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
    Write-Host "Created vcpkg binary cache directory: $cacheDir" -ForegroundColor Green
} else {
    Write-Host "vcpkg binary cache directory exists: $cacheDir" -ForegroundColor Cyan
}

# 设置当前会话的环境变量
$env:VCPKG_BINARY_SOURCES = "clear;files,$cacheDir,readwrite"
Write-Host "Enabled vcpkg binary cache for current session" -ForegroundColor Green

# 设置系统级环境变量（需要管理员权限）
try {
    [System.Environment]::SetEnvironmentVariable(
        "VCPKG_BINARY_SOURCES",
        "clear;files,$cacheDir,readwrite",
        [System.EnvironmentVariableTarget]::User
    )
    Write-Host "Set user environment variable (permanent)" -ForegroundColor Green
} catch {
    Write-Host "Warning: Could not set permanent environment variable" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Cache location: $cacheDir" -ForegroundColor Yellow
Write-Host "This will prevent Qt from being recompiled every time!" -ForegroundColor Green
