# 清理脚本 - 清理编译产物但保留vcpkg缓存

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Water Test System - Clean" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if (Test-Path "build") {
    Write-Host "Cleaning build artifacts (keeping vcpkg packages)..." -ForegroundColor Yellow
    
    # 删除编译产物，但保留vcpkg_installed
    Remove-Item "build/CMakeFiles" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "build/*.vcxproj*" -Force -ErrorAction SilentlyContinue
    Remove-Item "build/cmake_install.cmake" -Force -ErrorAction SilentlyContinue
    Remove-Item "build/CMakeCache.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item "build/bin" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "build/*.dir" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "build/WaterTestSystem.sln" -Force -ErrorAction SilentlyContinue
    
    Write-Host "Cleaned successfully! Qt packages preserved." -ForegroundColor Green
} else {
    Write-Host "No build directory found." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Run scripts\build.ps1 to rebuild the project" -ForegroundColor Cyan
