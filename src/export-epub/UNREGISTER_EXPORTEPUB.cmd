@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
set "DLL=%ROOT%out\Release\ExportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%out\Win32\Release\ExportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%ExportEPUB.dll"

set "REGSVR=%WINDIR%\SysWOW64\regsvr32.exe"
if not exist "%REGSVR%" set "REGSVR=%WINDIR%\System32\regsvr32.exe"

if exist "%DLL%" (
    echo Unregistering COM DLL:
    echo "%DLL%"
    "%REGSVR%" /u "%DLL%"
    exit /b %ERRORLEVEL%
)

echo ExportEPUB.dll not found. Trying to unregister by known output path anyway.
"%REGSVR%" /u "%ROOT%out\Release\ExportEPUB.dll"
exit /b %ERRORLEVEL%
