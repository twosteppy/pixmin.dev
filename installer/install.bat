@echo off
setlocal

set REPO_ROOT=%~dp0..
set PYTHON_CMD=

for %%P in (python3.exe python.exe) do (
    where %%P >nul 2>&1
    if not errorlevel 1 (
        set PYTHON_CMD=%%P
        goto :found
    )
)

echo [!] python 3.10+ not found. download from https://www.python.org/downloads/
pause
exit /b 1

:found
%PYTHON_CMD% "%REPO_ROOT%\installer\install.py" %*
if errorlevel 1 (
    echo.
    echo [!] installer failed. check output above.
    pause
    exit /b 1
)
pause
