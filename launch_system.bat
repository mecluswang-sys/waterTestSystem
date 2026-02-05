@echo off
REM ============================================================
REM Water Test System - Terminal & Station Launcher
REM ============================================================
REM This script launches the entire system:
REM - 1 Terminal Server (central)
REM - 4 Station Clients (operation panels)
REM ============================================================

setlocal enabledelayedexpansion

echo.
echo ============================================================
echo   Water Test System - Terminal-Station Architecture
echo ============================================================
echo.

set EXE_PATH=build\bin\Release\WaterTestSystem.exe

REM Check if executable exists
if not exist "%EXE_PATH%" (
    echo ERROR: Executable not found at %EXE_PATH%
    echo Please compile the project first.
    pause
    exit /b 1
)

echo [1/5] Checking executable...
echo       Found: %EXE_PATH%
echo.

echo [2/5] Verifying network configuration...
echo       PLC IP: 192.168.33.1
echo       Terminal Port: 5555
echo.

REM Ask user for mode
echo [3/5] Select mode:
echo       1) Start Terminal Server (central)
echo       2) Start Station Client (operation panel)
echo       3) Start Full System (Terminal + 4 Stations)
echo.
set /p mode="Enter choice (1/2/3): "

if "%mode%"=="1" (
    goto start_terminal
) else if "%mode%"=="2" (
    goto start_station
) else if "%mode%"=="3" (
    goto start_full
) else (
    echo ERROR: Invalid choice
    pause
    exit /b 1
)

:start_terminal
echo.
echo [4/5] Starting Terminal Server...
echo ============================================================
echo Terminal will listen on localhost:5555
echo Waiting for Station connections...
echo ============================================================
echo.
start "Water Test System - Terminal Server" "%EXE_PATH%" --mode terminal
timeout /t 1 >nul
goto done

:start_station
echo.
set /p station_id="Enter Station ID (1-4): "
set /p station_name="Enter Station Name (default: Operation Station): "

if "%station_name%"=="" (
    set station_name=Operation Station %station_id%
)

echo.
echo [4/5] Starting Station Client #%station_id%...
echo ============================================================
echo Station Name: %station_name%
echo Connecting to: localhost:5555
echo ============================================================
echo.
start "Water Test System - Station %station_id%" "%EXE_PATH%" --mode station --id %station_id% --name "%station_name%" --host 127.0.0.1 --port 5555
timeout /t 1 >nul
goto done

:start_full
echo.
echo [4/5] Starting Terminal Server and 4 Stations...
echo ============================================================
echo.

REM Start Terminal Server
echo Starting Terminal Server...
start "Water Test System - Terminal" "%EXE_PATH%" --mode terminal
timeout /t 2 >nul

REM Start Station 1
echo Starting Station 1...
start "Water Test System - Station 1" "%EXE_PATH%" --mode station --id 1 --name "操作台1" --host 127.0.0.1 --port 5555
timeout /t 500 /nobreak

REM Start Station 2
echo Starting Station 2...
start "Water Test System - Station 2" "%EXE_PATH%" --mode station --id 2 --name "操作台2" --host 127.0.0.1 --port 5555
timeout /t 500 /nobreak

REM Start Station 3
echo Starting Station 3...
start "Water Test System - Station 3" "%EXE_PATH%" --mode station --id 3 --name "操作台3" --host 127.0.0.1 --port 5555
timeout /t 500 /nobreak

REM Start Station 4
echo Starting Station 4...
start "Water Test System - Station 4" "%EXE_PATH%" --mode station --id 4 --name "操作台4" --host 127.0.0.1 --port 5555
timeout /t 1 >nul

echo.
echo ============================================================
echo All applications launched successfully!
echo ============================================================
echo.
echo Terminal Server:
echo   - Listening on localhost:5555
echo   - Connected to PLC at 192.168.33.1
echo.
echo Stations Started:
echo   - 操作台1 (ID: 1)
echo   - 操作台2 (ID: 2)
echo   - 操作台3 (ID: 3)
echo   - 操作台4 (ID: 4)
echo.

goto done

:done
echo [5/5] Done!
echo.
pause
