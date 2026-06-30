@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem Fast inventory pass for a large FB2 folder. It does not create DOCX files.
rem Edit SOURCE_DIR and OUTPUT_DIR before running.

set "SOURCE_DIR=D:\fb2_large_input"
set "OUTPUT_DIR=D:\fb2_large_output_docx"
set "LOG_DIR=%OUTPUT_DIR%\_logs_dryrun"
set "RECURSIVE=1"
set "PROGRESS_EVERY=100"

set "SCRIPT_DIR=%~dp0"
set "BATCH_EXE=%SCRIPT_DIR%out\Release\ExportDOCXBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%ExportDOCXBatch.exe"
if not exist "%BATCH_EXE%" (
  echo ERROR: ExportDOCXBatch.exe not found. Build first: BUILD_WIN32_RELEASE.cmd
  exit /b 2
)

mkdir "%OUTPUT_DIR%" >nul 2>nul
mkdir "%LOG_DIR%" >nul 2>nul

set "RECURSE_ARG="
if "%RECURSIVE%"=="1" set "RECURSE_ARG=-Recurse"

set "CSV_LOG=%LOG_DIR%\ExportDOCX_dryrun_log.csv"
set "SUMMARY_TXT=%LOG_DIR%\summary.txt"
set "HTML_REPORT=%LOG_DIR%\report.html"
set "CONSOLE_LOG=%LOG_DIR%\console.log"

"%BATCH_EXE%" "%SOURCE_DIR%" "%OUTPUT_DIR%" %RECURSE_ARG% -DryRun -ProgressEvery %PROGRESS_EVERY% -Log "%CSV_LOG%" -Summary "%SUMMARY_TXT%" -HtmlReport "%HTML_REPORT%" > "%CONSOLE_LOG%" 2>&1
set "EXIT_CODE=%ERRORLEVEL%"

echo Dry-run finished with code: %EXIT_CODE%
echo Summary: %SUMMARY_TXT%
echo CSV:     %CSV_LOG%
exit /b %EXIT_CODE%
