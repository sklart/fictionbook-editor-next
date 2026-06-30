@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem ============================================================================
rem ImportEPUB test runner and package creator
rem ============================================================================
rem Edit these variables before running the script.
rem INPUT_DIR  - folder with source EPUB files.
rem OUTPUT_DIR - folder where converted FB2 files and CSV report will be written.
rem PACKAGE_DIR - folder where the final ZIP package will be created.
rem ============================================================================

set "INPUT_DIR=D:\epub_test_suite"
set "OUTPUT_DIR=D:\epub_test_suite_fb2"
set "PACKAGE_DIR=D:\epub_import_test_packages"

rem Conversion options for ImportEPUBBatch.exe.
rem Recommended test mode: recursive, preserve source tree, overwrite old results,
rem collect statistics, write logs, convert SVG to PNG.
set "IMPORT_OPTIONS=--recursive --preserve-tree --overwrite --stats --log --svg png"

rem Set to 1 to build ImportEPUB.dll and ImportEPUBBatch.exe automatically if EXE is missing.
set "AUTO_BUILD_IF_MISSING=1"

rem Set to 1 to include source EPUB files in the ZIP package.
set "PACK_SOURCE_EPUB=1"

rem Set to 1 to include converted FB2 files in the ZIP package.
set "PACK_CONVERTED_FB2=1"

rem Set to 1 to include ImportEPUB logs in the ZIP package.
set "PACK_LOGS=1"

rem Set to 1 to open HTML report after conversion.
set "OPEN_REPORT_AFTER_RUN=1"

rem Set to 1 to run CHECK_DEPENDENCIES.cmd before conversion.
set "CHECK_DEPENDENCIES_BEFORE_RUN=0"

rem ============================================================================

set "SCRIPT_DIR=%~dp0"
set "BATCH_EXE=%SCRIPT_DIR%out\Release\ImportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%out\ImportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%ImportEPUBBatch.exe"

set "PLUGIN_DLL=%SCRIPT_DIR%out\Release\ImportEPUB.dll"
if not exist "%PLUGIN_DLL%" set "PLUGIN_DLL=%SCRIPT_DIR%out\ImportEPUB.dll"
if not exist "%PLUGIN_DLL%" set "PLUGIN_DLL=%SCRIPT_DIR%ImportEPUB.dll"

set "LUNASVG_DLL=%SCRIPT_DIR%out\Release\ImportEPUBLunaSVG.dll"
if not exist "%LUNASVG_DLL%" set "LUNASVG_DLL=%SCRIPT_DIR%out\ImportEPUBLunaSVG.dll"
if not exist "%LUNASVG_DLL%" set "LUNASVG_DLL=%SCRIPT_DIR%ImportEPUBLunaSVG.dll"

if not exist "%INPUT_DIR%" (
  echo ERROR: INPUT_DIR does not exist:
  echo   "%INPUT_DIR%"
  exit /b 1
)

if not exist "%BATCH_EXE%" (
  if "%AUTO_BUILD_IF_MISSING%"=="1" (
    echo ImportEPUBBatch.exe not found. Trying to build it...
    call "%SCRIPT_DIR%BUILD_BATCH_WIN32_RELEASE.cmd"
    if errorlevel 1 (
      echo ERROR: build failed.
      exit /b 1
    )
    set "BATCH_EXE=%SCRIPT_DIR%out\Release\ImportEPUBBatch.exe"
    set "PLUGIN_DLL=%SCRIPT_DIR%out\Release\ImportEPUB.dll"
  )
)

if not exist "%BATCH_EXE%" (
  echo ERROR: ImportEPUBBatch.exe not found.
  echo Expected:
  echo   "%SCRIPT_DIR%out\Release\ImportEPUBBatch.exe"
  exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%PACKAGE_DIR%" mkdir "%PACKAGE_DIR%"

for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"

set "REPORT_PATH=%OUTPUT_DIR%\ImportEPUBBatch_report.csv"
set "REPORT_HTML=%OUTPUT_DIR%\ImportEPUBBatch_report.html"
set "PACKAGE_WORK=%PACKAGE_DIR%\ImportEPUB_test_%STAMP%"
set "PACKAGE_ZIP=%PACKAGE_DIR%\ImportEPUB_test_%STAMP%.zip"

if exist "%PACKAGE_WORK%" rmdir /s /q "%PACKAGE_WORK%"
mkdir "%PACKAGE_WORK%"
mkdir "%PACKAGE_WORK%\report"
mkdir "%PACKAGE_WORK%\source_epub"
mkdir "%PACKAGE_WORK%\converted_fb2"
mkdir "%PACKAGE_WORK%\logs"
mkdir "%PACKAGE_WORK%\binaries"

set "CONVERT_CMD="%BATCH_EXE%" --batch "%INPUT_DIR%" "%OUTPUT_DIR%" %IMPORT_OPTIONS% --report "%REPORT_PATH%""

echo ============================================================================
echo ImportEPUB batch conversion
echo ============================================================================
echo Input:      "%INPUT_DIR%"
echo Output:     "%OUTPUT_DIR%"
echo Report:     "%REPORT_PATH%"
echo EXE:        "%BATCH_EXE%"
if exist "%PLUGIN_DLL%" (echo Plugin DLL: "%PLUGIN_DLL%") else (echo Plugin DLL: not found)
if exist "%LUNASVG_DLL%" (echo LunaSVG DLL: "%LUNASVG_DLL%") else (echo LunaSVG DLL: not found - SVG placeholders will be used)
echo Options:    %IMPORT_OPTIONS%
echo ============================================================================

if "%CHECK_DEPENDENCIES_BEFORE_RUN%"=="1" (
  call "%SCRIPT_DIR%CHECK_DEPENDENCIES.cmd"
  echo.
)

%BATCH_EXE% --batch "%INPUT_DIR%" "%OUTPUT_DIR%" %IMPORT_OPTIONS% --report "%REPORT_PATH%"
set "CONVERT_EXIT=%ERRORLEVEL%"

echo.
echo Conversion exit code: %CONVERT_EXIT%
echo.
echo Preparing package folder...

(
  echo ImportEPUB test package
  echo Generated: %DATE% %TIME%
  echo.
  echo Input EPUB folder:
  echo %INPUT_DIR%
  echo.
  echo Output FB2 folder:
  echo %OUTPUT_DIR%
  echo.
  echo Report:
  echo %REPORT_PATH%
  echo.
  echo Batch EXE:
  echo %BATCH_EXE%
  echo.
  echo Import plugin DLL:
  if exist "%PLUGIN_DLL%" (echo %PLUGIN_DLL%) else (echo NOT FOUND)
  echo.
  echo Optional LunaSVG adapter DLL:
  if exist "%LUNASVG_DLL%" (echo %LUNASVG_DLL%) else (echo NOT FOUND - SVG png/jpg modes will use placeholders)
  echo.
  echo Options:
  echo %IMPORT_OPTIONS%
  echo.
  echo Conversion exit code:
  echo %CONVERT_EXIT%
  echo.
  echo Recommended contents for analysis:
  echo - source_epub: original EPUB files
  echo - converted_fb2: converted FB2 files
  echo - logs: ImportEPUB logs
  echo - report: ImportEPUBBatch_report.csv and ImportEPUBBatch_report.html
  echo - binaries: ImportEPUB.dll, ImportEPUBBatch.exe, optional ImportEPUBLunaSVG.dll
) > "%PACKAGE_WORK%\README_TEST_PACKAGE.txt"

"%BATCH_EXE%" --version > "%PACKAGE_WORK%\ImportEPUBBatch_version.txt" 2>&1
if exist "%PLUGIN_DLL%" copy /y "%PLUGIN_DLL%" "%PACKAGE_WORK%\binaries\" >nul
if exist "%BATCH_EXE%" copy /y "%BATCH_EXE%" "%PACKAGE_WORK%\binaries\" >nul
if exist "%LUNASVG_DLL%" copy /y "%LUNASVG_DLL%" "%PACKAGE_WORK%\binaries\" >nul

if "%PACK_SOURCE_EPUB%"=="1" (
  echo Copying source EPUB files...
  call :RunRobo "%INPUT_DIR%" "%PACKAGE_WORK%\source_epub" "*.epub"
)

if "%PACK_CONVERTED_FB2%"=="1" (
  echo Copying converted FB2 files...
  call :RunRobo "%OUTPUT_DIR%" "%PACKAGE_WORK%\converted_fb2" "*.fb2"
)

if "%PACK_LOGS%"=="1" (
  echo Copying ImportEPUB logs from source folder...
  call :RunRobo "%INPUT_DIR%" "%PACKAGE_WORK%\logs" "*.ImportEPUB.log"
  echo Copying ImportEPUB logs from output folder...
  call :RunRobo "%OUTPUT_DIR%" "%PACKAGE_WORK%\logs" "*.ImportEPUB.log"
)

if exist "%REPORT_PATH%" copy /y "%REPORT_PATH%" "%PACKAGE_WORK%\report\" >nul
if exist "%REPORT_HTML%" copy /y "%REPORT_HTML%" "%PACKAGE_WORK%\report\" >nul
if exist "%OUTPUT_DIR%\ImportEPUBBatch_report.csv" copy /y "%OUTPUT_DIR%\ImportEPUBBatch_report.csv" "%PACKAGE_WORK%\report\" >nul
if exist "%OUTPUT_DIR%\ImportEPUBBatch_report.html" copy /y "%OUTPUT_DIR%\ImportEPUBBatch_report.html" "%PACKAGE_WORK%\report\" >nul

if exist "%PACKAGE_ZIP%" del /f /q "%PACKAGE_ZIP%"

echo Creating ZIP package...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Compress-Archive -Path '%PACKAGE_WORK%\*' -DestinationPath '%PACKAGE_ZIP%' -Force"

if errorlevel 1 (
  echo ERROR: failed to create ZIP package.
  echo Package folder remains here:
  echo   "%PACKAGE_WORK%"
  exit /b 1
)

echo.
echo ============================================================================
echo Done.
echo ZIP package:
echo   "%PACKAGE_ZIP%"
echo Package folder:
echo   "%PACKAGE_WORK%"
echo ============================================================================

if "%OPEN_REPORT_AFTER_RUN%"=="1" if exist "%REPORT_HTML%" (
  start "" "%REPORT_HTML%"
)

if not "%CONVERT_EXIT%"=="0" (
  echo WARNING: conversion returned non-zero exit code: %CONVERT_EXIT%
  echo The ZIP package was still created for diagnostics.
)

exit /b %CONVERT_EXIT%

:RunRobo
set "ROBO_SRC=%~1"
set "ROBO_DST=%~2"
set "ROBO_MASK=%~3"
if not exist "%ROBO_DST%" mkdir "%ROBO_DST%"
robocopy "%ROBO_SRC%" "%ROBO_DST%" %ROBO_MASK% /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul
set "ROBO_EXIT=%ERRORLEVEL%"
if %ROBO_EXIT% GEQ 8 (
  echo WARNING: robocopy failed. Source="%ROBO_SRC%" Mask="%ROBO_MASK%" Exit=%ROBO_EXIT%
)
exit /b 0
