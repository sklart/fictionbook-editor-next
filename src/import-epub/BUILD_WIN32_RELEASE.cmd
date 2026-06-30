@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

set "SLN=%~dp0ImportEPUB.sln"
set "MSBUILD="

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
)

if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD set "MSBUILD=msbuild.exe"


echo Building ImportEPUB and ImportEPUBBatch: Release ^| Win32
echo "%MSBUILD%" "%SLN%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32
"%MSBUILD%" "%SLN%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PreferredToolArchitecture=x86
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)

echo.
echo Build completed.
echo Output candidates:
if exist "%~dp0out\Release\ImportEPUB.dll" echo   %~dp0out\Release\ImportEPUB.dll
if exist "%~dp0out\Release\ImportEPUBBatch.exe" echo   %~dp0out\Release\ImportEPUBBatch.exe
if exist "%~dp0out\ImportEPUB.dll" echo   %~dp0out\ImportEPUB.dll
if exist "%~dp0out\ImportEPUBBatch.exe" echo   %~dp0out\ImportEPUBBatch.exe
pause
