@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo Close FB Editor before registration.
pause

call "%~dp0BUILD_WIN32_RELEASE.cmd"
if errorlevel 1 exit /b 1

set "DLL="
if exist "%~dp0out\Release\ExportDOCX.dll" set "DLL=%~dp0out\Release\ExportDOCX.dll"
if not defined DLL if exist "%~dp0out\Win32\Release\ExportDOCX.dll" set "DLL=%~dp0out\Win32\Release\ExportDOCX.dll"

if not defined DLL (
  echo ExportDOCX.dll not found after build.
  pause
  exit /b 1
)

echo.
echo Registering 32-bit COM DLL:
echo "%DLL%"
C:\Windows\SysWOW64\regsvr32.exe "%DLL%"
if errorlevel 1 (
  echo Registration failed.
  pause
  exit /b 1
)

echo.
echo Done. Restart FB Editor.
pause
