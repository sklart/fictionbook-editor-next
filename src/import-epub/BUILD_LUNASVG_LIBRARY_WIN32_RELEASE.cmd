@echo off
chcp 65001 >nul
setlocal EnableExtensions
cd /d "%~dp0"

set "LUNASVG_DIR=%~dp0thirdparty\lunasvg"
set "PLUTOVG_PROJ=%LUNASVG_DIR%\plutovg.vcxproj"
set "LUNASVG_PROJ=%LUNASVG_DIR%\lunasvg.vcxproj"
set "MSBUILD="

if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
)
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD set "MSBUILD=msbuild.exe"

if not exist "%PLUTOVG_PROJ%" (
  echo ERROR: not found: %PLUTOVG_PROJ%
  pause
  exit /b 1
)
if not exist "%LUNASVG_PROJ%" (
  echo ERROR: not found: %LUNASVG_PROJ%
  pause
  exit /b 1
)

echo Building PlutoVG static library: Release ^| Win32
echo "%MSBUILD%" "%PLUTOVG_PROJ%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32
"%MSBUILD%" "%PLUTOVG_PROJ%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PreferredToolArchitecture=x86
if errorlevel 1 (
  echo.
  echo plutovg.lib build failed.
  pause
  exit /b 1
)

echo.
echo Building LunaSVG static library: Release ^| Win32
echo "%MSBUILD%" "%LUNASVG_PROJ%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32
"%MSBUILD%" "%LUNASVG_PROJ%" /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PreferredToolArchitecture=x86
if errorlevel 1 (
  echo.
  echo lunasvg.lib build failed.
  pause
  exit /b 1
)

echo.
echo LunaSVG libraries built.
if exist "%LUNASVG_DIR%\lib\Win32\Release\plutovg.lib" echo   %LUNASVG_DIR%\lib\Win32\Release\plutovg.lib
if exist "%LUNASVG_DIR%\lib\Win32\Release\lunasvg.lib" echo   %LUNASVG_DIR%\lib\Win32\Release\lunasvg.lib
pause
