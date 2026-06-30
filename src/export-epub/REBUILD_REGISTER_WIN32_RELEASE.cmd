@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

echo Close FB Editor before registration.
if /i not "%NO_PAUSE%"=="1" pause

set "NO_PAUSE=1"
call "%~dp0BUILD_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

set "DLL="
if exist "%~dp0out\Release\ExportEPUB.dll" set "DLL=%~dp0out\Release\ExportEPUB.dll"
if not defined DLL if exist "%~dp0out\Win32\Release\ExportEPUB.dll" set "DLL=%~dp0out\Win32\Release\ExportEPUB.dll"
if not defined DLL if exist "%~dp0ExportEPUB.dll" set "DLL=%~dp0ExportEPUB.dll"

if not defined DLL (
  echo ExportEPUB.dll not found after build.
  if /i not "%NO_PAUSE%"=="1" pause
  exit /b 1
)

set "REGSVR=%WINDIR%\SysWOW64\regsvr32.exe"
if not exist "%REGSVR%" set "REGSVR=%WINDIR%\System32\regsvr32.exe"

echo.
echo Registering COM DLL:
echo "%DLL%"
"%REGSVR%" "%DLL%"
if errorlevel 1 (
  echo Registration failed.
  exit /b 1
)

echo.
echo Done. Restart FB Editor.
if /i not "%NO_PAUSE%"=="1" pause
exit /b 0
