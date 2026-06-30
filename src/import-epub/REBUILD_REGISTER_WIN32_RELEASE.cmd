@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo Close FB Editor before registration.
pause

call "%~dp0BUILD_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

call "%~dp0REGISTER_IMPORTEPUB.cmd"
if errorlevel 1 exit /b 1

echo.
echo Done. Restart FB Editor.
pause
