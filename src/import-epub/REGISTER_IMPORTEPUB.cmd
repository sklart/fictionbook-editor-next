@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
set "CLSID={D4B1B165-4D93-4F2D-8C8A-2D0C649431A1}"
set "REGEXE=%SystemRoot%\System32\reg.exe"

set "DLL=%ROOT%out\Release\ImportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%out\ImportEPUB.dll"
if not exist "%DLL%" set "DLL=%ROOT%ImportEPUB.dll"

if not exist "%DLL%" (
    echo ImportEPUB.dll not found.
    echo Run BUILD_WIN32_RELEASE.cmd first.
    exit /b 2
)

net session >nul 2>nul
if errorlevel 1 (
  echo ERROR: this script must be run as Administrator.
  echo Right-click it and choose "Run as administrator".
  exit /b 1
)

echo Registering ImportEPUB for 32-bit FB Editor:
echo %DLL%

"%REGEXE%" add "HKLM\Software\Classes\CLSID\%CLSID%" /ve /t REG_SZ /d "FBE EPUB Import Plugin" /f /reg:32 >nul
if errorlevel 1 exit /b 1
"%REGEXE%" add "HKLM\Software\Classes\CLSID\%CLSID%\InprocServer32" /ve /t REG_SZ /d "%DLL%" /f /reg:32 >nul
if errorlevel 1 exit /b 1
"%REGEXE%" add "HKLM\Software\Classes\CLSID\%CLSID%\InprocServer32" /v ThreadingModel /t REG_SZ /d "Apartment" /f /reg:32 >nul
if errorlevel 1 exit /b 1

set "IMPORT_EPUB_DLL=%DLL%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$clsid=$env:CLSID; $dll=$env:IMPORT_EPUB_DLL; $k='HKCU:\Software\FBETeam\FictionBook Editor\Plugins\' + $clsid; New-Item -Path $k -Force | Out-Null; Set-ItemProperty -Path $k -Name Type -Value 'Import'; Set-ItemProperty -Path $k -Name Menu -Value ([string]([char]0x0438)+[char]0x0437+' &EPUB...'); Set-ItemProperty -Path $k -Name Icon -Value ($dll + ',0')"
if errorlevel 1 exit /b %errorlevel%

echo Done. Restart FB Editor.
exit /b 0
