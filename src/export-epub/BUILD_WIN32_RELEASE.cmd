@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

set "SLN=%~dp0ExportEPUB.sln"
set "MSBUILD="

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
)

if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD set "MSBUILD=msbuild.exe"

set "LOG_DIR=%~dp0build_logs"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>nul
for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "LOG_FILE=%LOG_DIR%\build_%STAMP%_Release_Win32.log"
set "BINLOG_FILE=%LOG_DIR%\build_%STAMP%_Release_Win32.binlog"

echo Building ExportEPUB solution: Release ^| Win32
echo MSBuild: "%MSBUILD%"
echo Log:     "%LOG_FILE%"
echo Binlog:  "%BINLOG_FILE%"
"%MSBUILD%" "%SLN%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /v:minimal /bl:"%BINLOG_FILE%" > "%LOG_FILE%" 2>&1
if errorlevel 1 (
  type "%LOG_FILE%"
  echo.
  echo Build failed. Full log: "%LOG_FILE%"
  call :maybe_pause
  exit /b 1
)

echo.
echo Build completed.
echo Full log: "%LOG_FILE%"
echo Binary log: "%BINLOG_FILE%"
echo Output candidates:
if exist "%~dp0out\Release\ExportEPUB.dll" echo   %~dp0out\Release\ExportEPUB.dll
if exist "%~dp0out\Release\ExportEPUBBatch.exe" echo   %~dp0out\Release\ExportEPUBBatch.exe
if exist "%~dp0out\Win32\Release\ExportEPUB.dll" echo   %~dp0out\Win32\Release\ExportEPUB.dll
if exist "%~dp0out\Win32\Release\ExportEPUBBatch.exe" echo   %~dp0out\Win32\Release\ExportEPUBBatch.exe
call :maybe_pause
exit /b 0

:maybe_pause
if /i "%NO_PAUSE%"=="1" exit /b 0
if /i "%~1"=="nopause" exit /b 0
pause
exit /b 0
