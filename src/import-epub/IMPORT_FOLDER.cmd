@echo off
setlocal EnableExtensions
chcp 65001 >nul

if "%~1"=="" goto usage
if "%~2"=="" goto usage

set "INPUT=%~1"
set "OUTPUT=%~2"

set "BATCH_EXE=%~dp0out\Release\ImportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0out\ImportEPUBBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0ImportEPUBBatch.exe"

if not exist "%BATCH_EXE%" (
  echo ImportEPUBBatch.exe not found.
  echo Build Release^|Win32 first by running BUILD_WIN32_RELEASE.cmd.
  exit /b 1
)

shift
shift
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run_batch
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift
goto collect_args

:run_batch
"%BATCH_EXE%" --batch "%INPUT%" "%OUTPUT%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%

:usage
echo Usage:
echo   IMPORT_FOLDER.cmd "D:\Books_EPUB" "D:\Books_FB2"
echo.
echo Optional:
echo   --recursive
echo   --preserve-tree
echo   --overwrite
echo   --log
echo   --stats
echo   --dry-run
echo   --no-report
echo   --no-cover
echo   --no-images
echo   --no-notes
echo   --no-tables
echo   --no-lists
echo   --no-links
echo   --no-validate
exit /b 1
