param(
    [switch]$Rebuild,
    [switch]$KillExisting,
    [switch]$CleanCache
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$workspaceRoot = Split-Path -Parent $scriptDir

$debugDir = Join-Path $workspaceRoot "build/bin/Debug"
$exePath = Join-Path $debugDir "WaterTestSystem.exe"
$deployDir = Join-Path $workspaceRoot "deploy"
$rebuildScript = Join-Path $scriptDir "rebuild.ps1"
$workspaceConfig = Join-Path $workspaceRoot "config/system.conf"

$qtDebugPluginRootCandidates = @(
    (Join-Path $workspaceRoot "build/vcpkg_installed/x64-windows/debug/Qt6/plugins"),
    (Join-Path $workspaceRoot "vcpkg_installed/x64-windows/debug/Qt6/plugins")
)

function Write-Section([string]$title) {
    Write-Host ""
    Write-Host "=== $title ===" -ForegroundColor Cyan
}

function Clear-CMakeCache {
    Write-Section "Clear CMake Cache"

    $cacheFile = Join-Path $workspaceRoot "build/CMakeCache.txt"
    $cacheDir = Join-Path $workspaceRoot "build/CMakeFiles"
    $generatedDirs = @(
        (Join-Path $workspaceRoot "build/.cmake"),
        (Join-Path $workspaceRoot "build/x64"),
        (Join-Path $workspaceRoot "build/bin/Debug"),
        (Join-Path $workspaceRoot "build/bin/Release"),
        (Join-Path $workspaceRoot "build/WaterTestSystem_autogen")
    )

    if (Test-Path $cacheFile) {
        Remove-Item -Path $cacheFile -Force -ErrorAction SilentlyContinue
        Write-Host "Removed: build/CMakeCache.txt"
    }
    else {
        Write-Host "Skip missing: build/CMakeCache.txt" -ForegroundColor Yellow
    }

    if (Test-Path $cacheDir) {
        Remove-Item -Path $cacheDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "Removed: build/CMakeFiles"
    }
    else {
        Write-Host "Skip missing: build/CMakeFiles" -ForegroundColor Yellow
    }

    foreach ($dir in $generatedDirs) {
        if (Test-Path $dir) {
            Remove-Item -Path $dir -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "Removed: $($dir.Replace($workspaceRoot + '\\', ''))"
        }
    }
}

Push-Location $workspaceRoot

try {
    Write-Host "WaterTestSystem Debug Launcher" -ForegroundColor Green

    if ($CleanCache -or $Rebuild) {
        Clear-CMakeCache
        if ($CleanCache -and -not $Rebuild) {
            Write-Host "Cache cleanup finished." -ForegroundColor Green
            exit 0
        }
    }

    if ($Rebuild) {
        Write-Section "Rebuild Debug"
        & $rebuildScript -BuildType Debug
        if ($LASTEXITCODE -ne 0) {
            throw "Debug rebuild failed with exit code $LASTEXITCODE"
        }
    }

    if (-not (Test-Path $exePath)) {
        throw "Debug executable not found: $exePath"
    }

    if ($KillExisting) {
        Write-Section "Stop Existing Process"
        Get-Process -Name "WaterTestSystem" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }

    Write-Section "Sync Qt Plugins"
    $pluginDirs = @("platforms", "styles", "imageformats", "sqldrivers", "tls", "networkinformation")

    $qtDebugPluginRoot = $null
    foreach ($p in $qtDebugPluginRootCandidates) {
        if (Test-Path $p) {
            $qtDebugPluginRoot = $p
            break
        }
    }

    # 先清理旧插件目录，避免 release/debug 混拷导致平台插件初始化失败
    foreach ($name in $pluginDirs) {
        $dst = Join-Path $debugDir $name
        if (Test-Path $dst) {
            Remove-Item -Path $dst -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    foreach ($name in $pluginDirs) {
        $src = if ($qtDebugPluginRoot) { Join-Path $qtDebugPluginRoot $name } else { Join-Path $deployDir $name }
        if (Test-Path $src) {
            Copy-Item -Path $src -Destination $debugDir -Recurse -Force
            if ($qtDebugPluginRoot) {
                Write-Host "Copied(debug): $name"
            }
            else {
                Write-Host "Copied(deploy): $name"
            }
        } else {
            Write-Host "Skip missing: $name" -ForegroundColor Yellow
        }
    }

    $qtConf = Join-Path $deployDir "qt.conf"
    if (Test-Path $qtConf) {
        Copy-Item -Path $qtConf -Destination $debugDir -Force
        Write-Host "Copied: qt.conf"
    }
    else {
        $qtConfContent = "[Paths]`r`nPlugins=." 
        Set-Content -Path (Join-Path $debugDir "qt.conf") -Value $qtConfContent -Encoding ASCII
        Write-Host "Generated: qt.conf"
    }

    Write-Section "Sync Runtime Config"
    if (Test-Path $workspaceConfig) {
        $debugConfigDir = Join-Path $debugDir "config"
        New-Item -ItemType Directory -Path $debugConfigDir -Force | Out-Null
        Copy-Item -Path $workspaceConfig -Destination (Join-Path $debugConfigDir "system.conf") -Force
        Write-Host "Copied: config/system.conf"
    }
    else {
        Write-Host "Skip missing: config/system.conf" -ForegroundColor Yellow
    }

    # 强制平台插件路径，避免 Qt 从其他路径加载到不匹配插件
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $debugDir "platforms"
    $env:QT_PLUGIN_PATH = $debugDir
    $env:QT_QPA_PLATFORM = "windows"
    Write-Host "QT_QPA_PLATFORM_PLUGIN_PATH=$($env:QT_QPA_PLATFORM_PLUGIN_PATH)"

    Write-Section "Start Debug App"
    Start-Process -FilePath $exePath -WorkingDirectory $debugDir
    Start-Sleep -Seconds 1

    $proc = Get-Process -Name "WaterTestSystem" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $proc) {
        throw "Process did not start. Please check runtime dependencies or app logs."
    }

    Write-Host "Started PID: $($proc.Id)" -ForegroundColor Green
    Write-Host "Executable : $exePath"
}
catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
