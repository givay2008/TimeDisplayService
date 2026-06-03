@echo off
echo Uninstalling

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Please run as administrator.
    pause
    exit /b 1
)

sc stop TimeDisplayService

sc delete TimeDisplayService

echo Service uninstalled successfully!
pause
