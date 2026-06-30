@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "CLSID={D4B1B165-4D93-4F2D-8C8A-2D0C649431A1}"
set "REGEXE=%SystemRoot%\System32\reg.exe"
set "DLL=%~dp0out\Release\ImportEPUB.dll"
if not exist "%DLL%" set "DLL=%~dp0out\ImportEPUB.dll"

echo ================================================================
echo ImportEPUB registration check, Win32/x86 FBE
echo CLSID: %CLSID%
echo DLL:   %DLL%
echo ================================================================
echo.

echo [0] DLL file:
if exist "%DLL%" (
  echo Exists: "%DLL%"
) else (
  echo ERROR: DLL does not exist: "%DLL%"
)
echo.

echo [1] FBE plugin menu key:
"%REGEXE%" query "HKCU\Software\FBETeam\FictionBook Editor\Plugins\%CLSID%"
echo.

echo [2] HKLM 32-bit COM class key:
"%REGEXE%" query "HKLM\Software\Classes\CLSID\%CLSID%\InprocServer32" /reg:32
echo.

echo If the menu item is visible but FBE shows 80040154,
echo run REGISTER_IMPORTEPUB.cmd as Administrator, then restart FB Editor.
echo.
pause
