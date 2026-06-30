@echo off
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"

set "SLN=%~dp0ImportEPUB.sln"
set "CONFIG=Release"
set "PLATFORM=Win32"
set "TARGET=Rebuild"

rem Preferred MSBuild path for this project / user environment.
set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"

rem Fallback search for other Visual Studio installations.
if not exist "%MSBUILD%" (
    set "MSBUILD="
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MSBUILD=%%I"
    )
    if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    if not defined MSBUILD set "MSBUILD=msbuild.exe"
)

if not exist "%SLN%" (
    echo ImportEPUB.sln not found:
    echo "%SLN%"
    pause
    exit /b 2
)

echo ================================================================
echo Building ImportEPUBBatch package
echo Solution:      "%SLN%"
echo Configuration: %CONFIG%
echo Platform:      %PLATFORM%
echo MSBuild:       "%MSBUILD%"
echo ================================================================
echo.

call :BuildTarget ImportEPUB ImportEPUB.dll
if errorlevel 1 exit /b 1

call :BuildTarget ImportEPUBBatch ImportEPUBBatch.exe
if errorlevel 1 exit /b 1

echo.
echo Build completed successfully.
echo.
echo Output files:
if exist "%~dp0out\Release\ImportEPUB.dll" echo   "%~dp0out\Release\ImportEPUB.dll"
if exist "%~dp0out\Release\ImportEPUBBatch.exe" echo   "%~dp0out\Release\ImportEPUBBatch.exe"
if exist "%~dp0out\ImportEPUB.dll" echo   "%~dp0out\ImportEPUB.dll"
if exist "%~dp0out\ImportEPUBBatch.exe" echo   "%~dp0out\ImportEPUBBatch.exe"
echo.
pause
exit /b 0

:BuildTarget
set "PROJECT_TARGET=%~1"
set "DISPLAY_NAME=%~2"

echo Building %DISPLAY_NAME%: %CONFIG% ^| %PLATFORM%
echo "%MSBUILD%" "%SLN%" /m /t:%PROJECT_TARGET%:%TARGET% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM%
"%MSBUILD%" "%SLN%" /m /t:%PROJECT_TARGET%:%TARGET% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PreferredToolArchitecture=x86
if errorlevel 1 (
    echo.
    echo %DISPLAY_NAME% build failed.
    pause
    exit /b 1
)
echo.
exit /b 0
