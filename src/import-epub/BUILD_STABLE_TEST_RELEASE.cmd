@echo off
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"

echo ===============================================================
echo ImportEPUB stable test build
echo ===============================================================

call "%~dp0BUILD_BATCH_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

echo.
echo Building optional LunaSVG adapter...
call "%~dp0BUILD_LUNASVG_ALL_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

echo.
echo Checking dependencies...
call "%~dp0CHECK_DEPENDENCIES.cmd"
if errorlevel 1 exit /b 1

echo.
echo Build is ready for large-volume testing.
echo Run BIG_VOLUME_TEST.cmd after editing INPUT_DIR/OUTPUT_DIR.
exit /b 0
