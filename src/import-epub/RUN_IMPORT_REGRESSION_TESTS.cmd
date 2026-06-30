@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem ============================================================================
rem ImportEPUB regression smoke tests
rem ============================================================================
rem Edit these variables if your test suite is located elsewhere.
rem ============================================================================

set "INPUT_DIR=D:\epub_test_suite"
set "OUTPUT_DIR=D:\epub_test_suite_regression_fb2"
set "REPORT_PATH=%OUTPUT_DIR%\ImportEPUB_regression_report.csv"
set "IMPORT_OPTIONS=--recursive --preserve-tree --stats --log --svg png --flush-report-each --isolate-crashes"
rem 1 = resume mode: skip already converted FB2 files after crash/interruption.
rem 0 = clean overwrite run.
set "RESUME_MODE=1"
set "MAX_FILES=0"
set "AUTO_BUILD_IF_MISSING=1"
set "AUTO_BUILD_LUNASVG_IF_MISSING=0"

set "SCRIPT_DIR=%~dp0"
set "BATCH_EXE=%SCRIPT_DIR%out\Release\ImportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%out\ImportEPUBBatch.exe"
set "LUNASVG_DLL=%SCRIPT_DIR%out\Release\ImportEPUBLunaSVG.dll"
if not exist "%LUNASVG_DLL%" set "LUNASVG_DLL=%SCRIPT_DIR%out\ImportEPUBLunaSVG.dll"
set "REGRESSION_TXT=%OUTPUT_DIR%\REGRESSION_TEST_REPORT.txt"

if not exist "%INPUT_DIR%" (
  echo ERROR: INPUT_DIR does not exist:
  echo   "%INPUT_DIR%"
  exit /b 1
)

if not exist "%BATCH_EXE%" (
  if "%AUTO_BUILD_IF_MISSING%"=="1" (
    echo ImportEPUBBatch.exe not found. Building...
    call "%SCRIPT_DIR%BUILD_BATCH_WIN32_RELEASE.cmd"
    if errorlevel 1 exit /b 1
    set "BATCH_EXE=%SCRIPT_DIR%out\Release\ImportEPUBBatch.exe"
  )
)

if "%AUTO_BUILD_LUNASVG_IF_MISSING%"=="1" if not exist "%LUNASVG_DLL%" (
  echo ImportEPUBLunaSVG.dll not found. Building LunaSVG adapter...
  call "%SCRIPT_DIR%BUILD_LUNASVG_ALL_WIN32_RELEASE.cmd"
)

if not exist "%BATCH_EXE%" (
  echo ERROR: ImportEPUBBatch.exe not found.
  exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if "%RESUME_MODE%"=="0" (
  if exist "%REPORT_PATH%" del /f /q "%REPORT_PATH%" >nul 2>nul
  if exist "%REGRESSION_TXT%" del /f /q "%REGRESSION_TXT%" >nul 2>nul
)

echo ============================================================================
echo ImportEPUB regression tests
echo ============================================================================
echo Input:      "%INPUT_DIR%"
echo Output:     "%OUTPUT_DIR%"
echo Report:     "%REPORT_PATH%"
echo EXE:        "%BATCH_EXE%"
if exist "%LUNASVG_DLL%" (echo LunaSVG:    "%LUNASVG_DLL%") else (echo LunaSVG:    not found - SVG placeholders are expected)
echo Options:    %IMPORT_OPTIONS%
echo Resume:     %RESUME_MODE%
echo Max files:  %MAX_FILES%
echo ============================================================================

set "MAX_FILES_ARG="
if not "%MAX_FILES%"=="0" set "MAX_FILES_ARG=--max-files %MAX_FILES%"
set "RESUME_ARG=--skip-existing"
if "%RESUME_MODE%"=="0" set "RESUME_ARG=--overwrite"
"%BATCH_EXE%" --batch "%INPUT_DIR%" "%OUTPUT_DIR%" %IMPORT_OPTIONS% %RESUME_ARG% %MAX_FILES_ARG% --report "%REPORT_PATH%"
set "RUN_EXIT=%ERRORLEVEL%"

if exist "%LUNASVG_DLL%" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%RUN_IMPORT_REGRESSION_TESTS.ps1" -ReportPath "%REPORT_PATH%" -OutputPath "%REGRESSION_TXT%" -LunaSvgPresent 1
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%RUN_IMPORT_REGRESSION_TESTS.ps1" -ReportPath "%REPORT_PATH%" -OutputPath "%REGRESSION_TXT%" -LunaSvgPresent 0
)
set "TEST_EXIT=%ERRORLEVEL%"

echo.
echo Regression report:
echo   "%REGRESSION_TXT%"
if exist "%REGRESSION_TXT%" type "%REGRESSION_TXT%"

echo.
if not "%RUN_EXIT%"=="0" (
  echo ERROR: conversion exited with code %RUN_EXIT%.
  exit /b %RUN_EXIT%
)
exit /b %TEST_EXIT%
