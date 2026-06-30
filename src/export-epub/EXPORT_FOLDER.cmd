@echo off
setlocal EnableExtensions
chcp 65001 >nul

if "%~1"=="" goto usage
if "%~2"=="" goto usage

set "INPUT=%~1"
set "OUTPUT=%~2"

set "BATCH_EXE=%~dp0out\Release\ExportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0out\Win32\Release\ExportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0ExportEPUBBatch.exe"

if not exist "%BATCH_EXE%" (
  echo ExportEPUBBatch.exe not found.
  echo Build Release^|Win32 first by running BUILD_WIN32_RELEASE.cmd.
  exit /b 1
)

shift
shift
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run_batch
set "EXTRA_ARGS=%EXTRA_ARGS% "%~1""
shift
goto collect_args

:run_batch
"%BATCH_EXE%" --input "%INPUT%" --output "%OUTPUT%" --version 3 --recursive --overwrite %EXTRA_ARGS%
exit /b %ERRORLEVEL%

:usage
echo Usage:
echo   EXPORT_FOLDER.cmd "D:\Books" "D:\Books_EPUB"
echo.
echo Default command uses: --version 3 --recursive --overwrite
echo Additional ExportEPUBBatch.exe switches may be appended, for example:
echo   --version 2
echo   --version both
echo   --dry-run
echo   --flat-output
echo   --report-csv "D:\Books_EPUB\report.csv"
echo   --report-log "D:\Books_EPUB\export.log"
echo   --no-cover-page
echo   --no-annotation-page
echo   --no-ncx-fallback
echo   --log
exit /b 1
