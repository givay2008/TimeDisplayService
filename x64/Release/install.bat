@echo off
echo Installing

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Please run as administrator.
    pause
    exit /b 1
)

set SERVICE_PATH=%~dp0TimeDisplayService.exe
set UI_PATH=%~dp0TimeDisplayUI.exe

sc query TimeDisplayService >nul 2>&1
if %errorLevel% equ 0 (
    echo Service exists. Stopping service...
    sc stop TimeDisplayService
    timeout /t 2 /nobreak >nul

    echo Deleting existing service...
    sc delete TimeDisplayService
    timeout /t 2 /nobreak >nul
)

echo Creating service...
sc create TimeDisplayService binPath= "%SERVICE_PATH%" start= auto DisplayName= "Time Display Service"

sc description TimeDisplayService "Time Enhanced Display"

echo Configuring service permissions...
sc config TimeDisplayService type= own

echo Starting service...
sc start TimeDisplayService

echo Service installed and started successfully!

pause
