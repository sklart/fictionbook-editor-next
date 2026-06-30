@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
set "DLL=%ROOT%out\Release\ExportDOCX.dll"
if not exist "%DLL%" set "DLL=%ROOT%out\Win32\Release\ExportDOCX.dll"
if not exist "%DLL%" set "DLL=%ROOT%ExportDOCX.dll"

if not exist "%DLL%" (
    echo ExportDOCX.dll not found.
    exit /b 2
)

echo Unregistering 32-bit COM DLL:
echo %DLL%
C:\Windows\SysWOW64\regsvr32.exe /u "%DLL%"
exit /b %ERRORLEVEL%
