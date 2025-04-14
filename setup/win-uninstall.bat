@echo off
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrative privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

setlocal enabledelayedexpansion

set "found="
set "script_dir=%~dp0"

for %%c in (python python3 python2 py) do (
    echo Checking python executable: '%%c'
    where %%c >nul 2>&1 && %%c --version >nul 2>&1 && (
        set "found=%%c"
        goto :found
    )
)


:found
if defined found (
    echo Valid python executable found. Runnnig setup...
    %found% %script_dir%setup.py -s --uninstall
) else (
    echo Python is required to run the setup.
    exit /b 1
)

pause
