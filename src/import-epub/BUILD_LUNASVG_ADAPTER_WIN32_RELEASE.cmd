@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

set "SLN=%~dp0ImportEPUB.sln"
set "MSBUILD="

if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
)
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD set "MSBUILD=msbuild.exe"

set "LUNASVG_LIB=%~dp0thirdparty\lunasvg\lib\Win32\Release\lunasvg.lib"
set "PLUTOVG_LIB=%~dp0thirdparty\lunasvg\lib\Win32\Release\plutovg.lib"

if not exist "%LUNASVG_LIB%" (
  echo LunaSVG static libraries were not found. Building them first...
  call "%~dp0BUILD_LUNASVG_LIBRARY_WIN32_RELEASE.cmd"
  if errorlevel 1 exit /b 1
)
if not exist "%PLUTOVG_LIB%" (
  echo PlutoVG static library was not found after build.
  pause
  exit /b 1
)

echo Building optional ImportEPUBLunaSVG.dll: Release ^| Win32
echo.
echo This target links to local LunaSVG/PlutoVG static libraries.
echo See thirdparty\lunasvg\README_IMPORT_EPUB_BUILD.txt
echo.
"%MSBUILD%" "%SLN%" /m /t:ImportEPUBLunaSVG:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PreferredToolArchitecture=x86
if errorlevel 1 (
  echo.
  echo ImportEPUBLunaSVG.dll build failed.
  echo Check that LunaSVG/PlutoVG libraries were built successfully.
  pause
  exit /b 1
)

echo.
echo Build completed.
if exist "%~dp0out\Release\ImportEPUBLunaSVG.dll" echo   %~dp0out\Release\ImportEPUBLunaSVG.dll
pause
