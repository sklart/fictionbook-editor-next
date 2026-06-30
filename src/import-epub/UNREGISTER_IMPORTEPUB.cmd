@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "CLSID={D4B1B165-4D93-4F2D-8C8A-2D0C649431A1}"
set "REGEXE=%SystemRoot%\System32\reg.exe"

net session >nul 2>nul
if errorlevel 1 (
  echo ERROR: this script must be run as Administrator.
  echo Right-click it and choose "Run as administrator".
  exit /b 1
)

echo Unregistering ImportEPUB for 32-bit FB Editor...
"%REGEXE%" delete "HKLM\Software\Classes\CLSID\%CLSID%" /f /reg:32 >nul 2>nul
"%REGEXE%" delete "HKCU\Software\FBETeam\FictionBook Editor\Plugins\%CLSID%" /f >nul 2>nul

echo Done.
exit /b 0
