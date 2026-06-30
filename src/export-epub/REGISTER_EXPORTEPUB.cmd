@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
set "DLL=%ROOT%out\Release\ExportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%out\Win32\Release\ExportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%ExportEPUB.dll"

if not exist "%DLL%" (
    echo ExportEPUB.dll not found.
    echo Run BUILD_WIN32_RELEASE.cmd first.
    exit /b 2
)

set "REGSVR=%WINDIR%\SysWOW64\regsvr32.exe"
if not exist "%REGSVR%" set "REGSVR=%WINDIR%\System32\regsvr32.exe"

echo Registering COM DLL:
echo "%DLL%"
"%REGSVR%" "%DLL%"
exit /b %ERRORLEVEL%
