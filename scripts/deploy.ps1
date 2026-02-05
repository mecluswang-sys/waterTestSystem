# 部署脚本 - 打包程序到deploy目录

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Water Test System - Deploy" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$sourceDir = "build/bin/Release"
$deployDir = "deploy"

function Stop-WaterTestSystemIfRunning {
    $procs = Get-Process -Name "WaterTestSystem" -ErrorAction SilentlyContinue
    if ($null -ne $procs) {
        Write-Host "Stopping running WaterTestSystem..." -ForegroundColor Yellow
        $procs | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
}

# 检查编译产物是否存在
if (-not (Test-Path "$sourceDir/WaterTestSystem.exe")) {
    Write-Host "Error: WaterTestSystem.exe not found. Build the project first." -ForegroundColor Red
    exit 1
}

# 清理旧的部署目录
if (Test-Path $deployDir) {
    Write-Host "Cleaning old deployment..." -ForegroundColor Yellow
    try {
        Remove-Item -Path $deployDir -Recurse -Force -ErrorAction Stop
    } catch {
        Stop-WaterTestSystemIfRunning
        Remove-Item -Path $deployDir -Recurse -Force
    }
}

# 创建部署目录
New-Item -ItemType Directory -Path $deployDir -Force | Out-Null

# 复制可执行文件
Write-Host "Copying executable..." -ForegroundColor Cyan
Copy-Item -Path "$sourceDir/WaterTestSystem.exe" -Destination $deployDir

# 复制DLL文件
Write-Host "Copying DLLs..." -ForegroundColor Cyan
Copy-Item -Path "$sourceDir/*.dll" -Destination $deployDir

# 部署Qt依赖（优先使用 windeployqt）
Write-Host "Deploying Qt runtime (windeployqt or manual copy)..." -ForegroundColor Cyan

# 尝试自动定位 vcpkg_installed 下的 Qt 目录（支持 build/ 前缀或根目录 vcpkg_installed）
$qtBaseCandidates = @(
    "build/vcpkg_installed/x64-windows/Qt6",
    "vcpkg_installed/x64-windows/Qt6",
    "vcpkg_installed/x64-windows-release/Qt6"
)

$foundQtBase = $null
foreach ($c in $qtBaseCandidates) {
    if (Test-Path $c) { $foundQtBase = $c; break }
}

if ($foundQtBase) {
    $qtToolsDir = Join-Path $foundQtBase "..\tools\Qt6\bin"
    $pluginsSourceDir = Join-Path $foundQtBase "plugins"
} else {
    # 保守默认（与历史脚本兼容）
    $qtToolsDir = "build/vcpkg_installed/x64-windows/tools/Qt6/bin"
    $pluginsSourceDir = "build/vcpkg_installed/x64-windows/Qt6/plugins"
}

$windeploy = Join-Path $qtToolsDir "windeployqt.exe"

if (Test-Path $windeploy) {
    & $windeploy --release --dir $deployDir "$deployDir/WaterTestSystem.exe"
} else {
    Write-Host "windeployqt not found, falling back to manual plugin copy" -ForegroundColor Yellow

    # 平台插件
    New-Item -ItemType Directory -Path "$deployDir/platforms" -Force | Out-Null
    Copy-Item -Path "$pluginsSourceDir/platforms/*" -Destination "$deployDir/platforms/" -Recurse -Force -ErrorAction SilentlyContinue

    # 常见插件目录（按需）
    foreach ($sub in @("styles","imageformats","sqldrivers","tls","networkinformation")) {
        if (Test-Path (Join-Path $pluginsSourceDir $sub)) {
            New-Item -ItemType Directory -Path (Join-Path $deployDir $sub) -Force | Out-Null
            Copy-Item -Path (Join-Path $pluginsSourceDir "$sub\*") -Destination (Join-Path $deployDir $sub) -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# 复制配置文件
Write-Host "Copying configuration..." -ForegroundColor Cyan
New-Item -ItemType Directory -Path "$deployDir/config" -Force | Out-Null
if (Test-Path "config/system.conf") {
    Copy-Item -Path "config/system.conf" -Destination "$deployDir/config/"
}

# 创建日志目录
New-Item -ItemType Directory -Path "$deployDir/logs" -Force | Out-Null

# 创建启动脚本
Write-Host "Creating launcher..." -ForegroundColor Cyan
$startScript = @"
@echo off
REM Switch to the script directory
cd /d %~dp0
setlocal
REM Ensure Qt can locate platform plugins
set QT_PLUGIN_PATH=%~dp0\platforms
set QT_QPA_PLATFORM=windows
echo ========================================
echo    Water Test System v1.0
echo    Siemens S7-1200 PLC Monitor
echo ========================================
echo.
echo Launcher directory: %~dp0
for %%I in ("%~dp0WaterTestSystem.exe") do echo Executable: %%~fI
if not exist "%~dp0WaterTestSystem.exe" (
    echo ERROR: WaterTestSystem.exe not found next to launcher.
    pause
    exit /b 1
)
echo Starting application...
start "" WaterTestSystem.exe
endlocal
"@
$startScript | Out-File -FilePath "$deployDir/启动程序.bat" -Encoding ASCII

# 额外生成 ASCII 文件名，避免部分环境中文名显示乱码
$asciiStartScript = @"
@echo off
REM Switch to the script directory
cd /d %~dp0
setlocal
set QT_PLUGIN_PATH=%~dp0\platforms
set QT_QPA_PLATFORM=windows
echo ========================================
echo    Water Test System v1.0
echo    Siemens S7-1200 PLC Monitor
echo ========================================
echo.
echo Launcher directory: %~dp0
for %%I in ("%~dp0WaterTestSystem.exe") do echo Executable: %%~fI
if not exist "%~dp0WaterTestSystem.exe" (
    echo ERROR: WaterTestSystem.exe not found next to launcher.
    pause
    exit /b 1
)
echo Starting application...
start "" WaterTestSystem.exe
endlocal
"@
$asciiStartScript | Out-File -FilePath "$deployDir/start.bat" -Encoding ASCII

# 创建使用说明
$readme = @"
Water Test System - 使用说明
====================================

1. 运行程序
   - 双击 "启动程序.bat" 或 "WaterTestSystem.exe"

2. 配置PLC连接
   - 编辑 config/system.conf 文件
   - 修改 plc.ip 为您的PLC IP地址
   - 默认: 192.168.0.1

3. 连接PLC
   - 点击菜单 "文件 -> 连接"
   - 或点击工具栏的 "连接" 按钮

4. 查看实时数据
   - 压力传感器: 11个
   - 流量计: 4个
   - 电动阀: 11个
   - 变频泵: 2个

5. 状态颜色说明
   - 绿色: 在线
   - 红色: 故障
   - 黄色: 维护中
   - 灰色: 离线

技术支持: WaterTest Team
版本: 1.0
"@
$readme | Out-File -FilePath "$deployDir/使用说明.txt" -Encoding UTF8

# 英文版说明（ASCII 文件名）
$readmeEn = @"
Water Test System - How to Run
====================================

1. Run the app
    - Double-click "start.bat" or "WaterTestSystem.exe" (from deploy/)

2. Configure PLC connection
    - Edit config/system.conf
    - Set plc.ip to your PLC IP address

3. Connect to PLC
    - Menu: File -> Connect

4. Real-time view
    - Pressure sensors: 11
    - Flow meter: 1
    - Electric valves: 11
    - Frequency pumps: 2
    - Temperature: position 2 (if configured)

5. Status colors
    - Green: Online
    - Red: Fault
    - Yellow: Maintenance
    - Gray: Offline

Support: WaterTest Team
Version: 1.0
"@
$readmeEn | Out-File -FilePath "$deployDir/README.txt" -Encoding ASCII

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Deployment completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Deployment directory: $((Get-Item $deployDir).FullName)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Files:" -ForegroundColor Cyan
Get-ChildItem -Path $deployDir -Recurse -File | ForEach-Object {
    $relativePath = $_.FullName.Substring($PWD.Path.Length + $deployDir.Length + 2)
    Write-Host "  $relativePath"
}
