@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem ================================================================
rem ExportDOCX large-volume FB2 test runner
rem ================================================================
rem Edit only variables in this block before running.
rem The script is resumable and safe for overnight processing.
rem ================================================================

set "SOURCE_DIR=D:\fb2_large_input"
set "OUTPUT_DIR=D:\fb2_large_output_docx"
set "LOG_DIR=%OUTPUT_DIR%\_logs"

rem 1 = recurse subfolders, 0 = only SOURCE_DIR
set "RECURSIVE=1"

rem 1 = do not overwrite existing DOCX and continue previous run
set "RESUME_MODE=1"

rem Recommended for large testing.
set "RETRIES=2"
set "PROGRESS_EVERY=25"
set "VALIDATE_DOCX=1"
set "PRESERVE_DATES=1"
set "UNIQUE_NAMES=0"

rem Recommended for unattended large tests: isolate each file, so a single bad FB2 cannot hang the whole run.
set "ISOLATED_EXPORT=1"
set "TIMEOUT_SEC=120"

rem Optional filters. Leave empty to disable.
set "LIMIT="
set "MIN_SIZE="
set "MAX_SIZE="
set "MODIFIED_AFTER="
set "MODIFIED_BEFORE="

rem 1 = open HTML report when finished
set "OPEN_HTML=0"

rem ================================================================
rem Do not edit below this line unless you want to change workflow.
rem ================================================================

set "SCRIPT_DIR=%~dp0"
set "BATCH_EXE=%SCRIPT_DIR%out\Release\ExportDOCXBatch.exe"
set "DLL_PATH=%SCRIPT_DIR%out\Release\ExportDOCX.dll"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%ExportDOCXBatch.exe"
if not exist "%DLL_PATH%" set "DLL_PATH=%SCRIPT_DIR%ExportDOCX.dll"

if not exist "%BATCH_EXE%" (
  echo ERROR: ExportDOCXBatch.exe not found.
  echo Build first: BUILD_WIN32_RELEASE.cmd
  exit /b 2
)
if not exist "%DLL_PATH%" (
  echo ERROR: ExportDOCX.dll not found.
  echo Build first: BUILD_WIN32_RELEASE.cmd
  exit /b 2
)
if not exist "%SOURCE_DIR%" (
  echo ERROR: SOURCE_DIR does not exist: %SOURCE_DIR%
  exit /b 2
)

mkdir "%OUTPUT_DIR%" >nul 2>nul
mkdir "%LOG_DIR%" >nul 2>nul

set "RECURSE_ARG="
if "%RECURSIVE%"=="1" set "RECURSE_ARG=-Recurse"

set "RESUME_ARGS="
if "%RESUME_MODE%"=="1" set "RESUME_ARGS=-Resume -SkipIfNewer"

set "VALIDATE_ARG="
if "%VALIDATE_DOCX%"=="1" set "VALIDATE_ARG=-ValidateDocx"

set "PRESERVE_ARG="
if "%PRESERVE_DATES%"=="1" set "PRESERVE_ARG=-PreserveDates"

set "UNIQUE_ARG="
if "%UNIQUE_NAMES%"=="1" set "UNIQUE_ARG=-UniqueNames"

set "ISOLATED_ARG="
if "%ISOLATED_EXPORT%"=="1" set "ISOLATED_ARG=-IsolatedExport -TimeoutSec %TIMEOUT_SEC%"

set "FILTER_ARGS="
if not "%LIMIT%"=="" set "FILTER_ARGS=!FILTER_ARGS! -Limit %LIMIT%"
if not "%MIN_SIZE%"=="" set "FILTER_ARGS=!FILTER_ARGS! -MinSize %MIN_SIZE%"
if not "%MAX_SIZE%"=="" set "FILTER_ARGS=!FILTER_ARGS! -MaxSize %MAX_SIZE%"
if not "%MODIFIED_AFTER%"=="" set "FILTER_ARGS=!FILTER_ARGS! -ModifiedAfter %MODIFIED_AFTER%"
if not "%MODIFIED_BEFORE%"=="" set "FILTER_ARGS=!FILTER_ARGS! -ModifiedBefore %MODIFIED_BEFORE%"

set "CSV_LOG=%LOG_DIR%\ExportDOCX_batch_log.csv"
set "HTML_REPORT=%LOG_DIR%\report.html"
set "SUMMARY_TXT=%LOG_DIR%\summary.txt"
set "FAILED_TXT=%LOG_DIR%\failed.txt"
set "OK_TXT=%LOG_DIR%\ok.txt"
set "WARNINGS_FILES_TXT=%LOG_DIR%\warnings_files.txt"
set "CONSOLE_LOG=%LOG_DIR%\ExportDOCX_batch_console.log"

set "OPEN_ARG="
if "%OPEN_HTML%"=="1" set "OPEN_ARG=-OpenHtmlReport"

echo ================================================================
echo ExportDOCX large-volume FB2 test
echo ================================================================
echo Source:  %SOURCE_DIR%
echo Output:  %OUTPUT_DIR%
echo Logs:    %LOG_DIR%
echo.
echo The run is resumable. Current file markers are written to:
echo   %LOG_DIR%\ExportDOCX_current_file.txt
echo   %LOG_DIR%\ExportDOCX_progress_state.txt
echo.

"%BATCH_EXE%" "%SOURCE_DIR%" "%OUTPUT_DIR%" %RECURSE_ARG% %RESUME_ARGS% %VALIDATE_ARG% %PRESERVE_ARG% %UNIQUE_ARG% %ISOLATED_ARG% !FILTER_ARGS! -Retries %RETRIES% -ProgressEvery %PROGRESS_EVERY% -Dll "%DLL_PATH%" -Log "%CSV_LOG%" -HtmlReport "%HTML_REPORT%" -Summary "%SUMMARY_TXT%" -FailedList "%FAILED_TXT%" -OkList "%OK_TXT%" -WarningsFilesList "%WARNINGS_FILES_TXT%" -PrintFailed %OPEN_ARG% > "%CONSOLE_LOG%" 2>&1
set "EXPORT_EXIT=%ERRORLEVEL%"

rem Normalize console log to UTF-8 if needed.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%CONSOLE_LOG%';" ^
  "if(Test-Path -LiteralPath $p){" ^
  "  $b=[IO.File]::ReadAllBytes($p);" ^
  "  if($b.Length -ge 4 -and $b[1] -eq 0 -and $b[3] -eq 0){" ^
  "    $s=[Text.Encoding]::Unicode.GetString($b);" ^
  "    [IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($true));" ^
  "  }" ^
  "}"

echo.
echo Export finished with code: %EXPORT_EXIT%
echo Summary: %SUMMARY_TXT%
echo CSV:     %CSV_LOG%
echo HTML:    %HTML_REPORT%
echo Failed:  %FAILED_TXT%
echo Warnings files: %WARNINGS_FILES_TXT%
echo.

call "%SCRIPT_DIR%ANALYZE_LARGE_FB2_RESULTS.cmd" "%LOG_DIR%"
exit /b %EXPORT_EXIT%
