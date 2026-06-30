@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

call "%~dp0BUILD_LUNASVG_LIBRARY_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

call "%~dp0BUILD_LUNASVG_ADAPTER_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

pause
