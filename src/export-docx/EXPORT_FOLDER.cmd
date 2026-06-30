@echo off
setlocal EnableExtensions
chcp 65001 >nul

if "%~1"=="" goto usage
if "%~2"=="" goto usage

set "INPUT=%~1"
set "OUTPUT=%~2"

set "BATCH_EXE=%~dp0out\Release\ExportDOCXBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0out\Win32\Release\ExportDOCXBatch.exe"
if not exist "%BATCH_EXE%" set "BATCH_EXE=%~dp0ExportDOCXBatch.exe"

if not exist "%BATCH_EXE%" (
  echo ExportDOCXBatch.exe not found.
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
"%BATCH_EXE%" "%INPUT%" "%OUTPUT%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%

:usage
echo Usage:
echo   EXPORT_FOLDER.cmd "D:\Books" "D:\Books_DOCX"
echo.
echo Optional:
echo   -Recurse
echo   -NoOverwrite
echo   -DryRun
echo   -Flat
echo   -Quiet
echo   -OpenLog
echo   -AppendLog
echo   -Resume
echo   -SkipIfNewer
echo   -StopOnError
echo   -Retries 2
echo   -ProgressEvery 20
echo   -PreserveDates
echo   -UniqueNames
echo   -Limit 10
echo   -List "D:\lists\books.txt"
echo   -FailedList "D:\Books_DOCX\failed.txt"
echo   -OkList "D:\Books_DOCX\ok.txt"
echo   -Summary "D:\Books_DOCX\summary.txt"
echo   -HtmlReport "D:\Books_DOCX\report.html"
echo   -MinSize 10KB
echo   -MaxSize 80MB
echo   -ModifiedAfter 2026-01-01
echo   -ModifiedBefore 2026-06-25
echo   -Dll "D:\Path\ExportDOCX.dll"
echo   -Log "D:\Path\ExportDOCX_batch_log.csv"
exit /b 1
