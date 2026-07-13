@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "CLSID={3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}"
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
