@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

set "DLL_PROJECT=%~dp0ExportEPUB.vcxproj"
set "BATCH_PROJECT=%~dp0ExportEPUBBatch.vcxproj"
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
set "DLL_LOG=%LOG_DIR%\build_dll_%STAMP%_Release_Win32.log"
set "BATCH_LOG=%LOG_DIR%\build_batch_%STAMP%_Release_Win32.log"
set "DLL_BINLOG=%LOG_DIR%\build_dll_%STAMP%_Release_Win32.binlog"
set "BATCH_BINLOG=%LOG_DIR%\build_batch_%STAMP%_Release_Win32.binlog"

if not exist "%DLL_PROJECT%" (
  echo ERROR: project not found: "%DLL_PROJECT%"
  call :maybe_pause
  exit /b 1
)
if not exist "%BATCH_PROJECT%" (
  echo ERROR: project not found: "%BATCH_PROJECT%"
  call :maybe_pause
  exit /b 1
)

echo ===============================================================
echo Building ExportEPUB.dll and ExportEPUBBatch.exe
echo Configuration: Release
echo Platform:      Win32
echo MSBuild:       "%MSBUILD%"
echo DLL log:       "%DLL_LOG%"
echo Batch log:     "%BATCH_LOG%"
echo ===============================================================
echo.

echo Building ExportEPUB.dll...
"%MSBUILD%" "%DLL_PROJECT%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /v:minimal /bl:"%DLL_BINLOG%" > "%DLL_LOG%" 2>&1
if errorlevel 1 (
  type "%DLL_LOG%"
  echo.
  echo Build failed: ExportEPUB.dll
  echo Full log: "%DLL_LOG%"
  call :maybe_pause
  exit /b 1
)

if not exist "%~dp0out\Release\ExportEPUB.dll" (
  echo ERROR: ExportEPUB.dll was not created:
  echo   "%~dp0out\Release\ExportEPUB.dll"
  echo See log: "%DLL_LOG%"
  call :maybe_pause
  exit /b 1
)

echo Building ExportEPUBBatch.exe...
"%MSBUILD%" "%BATCH_PROJECT%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /v:minimal /bl:"%BATCH_BINLOG%" > "%BATCH_LOG%" 2>&1
if errorlevel 1 (
  type "%BATCH_LOG%"
  echo.
  echo Build failed: ExportEPUBBatch.exe
  echo Full log: "%BATCH_LOG%"
  call :maybe_pause
  exit /b 1
)

if not exist "%~dp0out\Release\ExportEPUBBatch.exe" (
  type "%BATCH_LOG%"
  echo.
  echo ERROR: ExportEPUBBatch.exe was not created:
  echo   "%~dp0out\Release\ExportEPUBBatch.exe"
  echo Full log: "%BATCH_LOG%"
  call :maybe_pause
  exit /b 1
)

echo.
echo Build completed:
echo   %~dp0out\Release\ExportEPUB.dll
echo   %~dp0out\Release\ExportEPUBBatch.exe
echo.
echo Logs:
echo   "%DLL_LOG%"
echo   "%BATCH_LOG%"
call :maybe_pause
exit /b 0

:maybe_pause
if /i "%NO_PAUSE%"=="1" exit /b 0
pause
exit /b 0
