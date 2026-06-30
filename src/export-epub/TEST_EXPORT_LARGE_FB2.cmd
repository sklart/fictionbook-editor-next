@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem ============================================================
rem ExportEPUB large-volume FB2 -> EPUB test run
rem ============================================================
rem Purpose:
rem   Convert a large FB2 folder, keep resumable logs, run EPUBCheck
rem   and create a compact archive with logs and only problem files.
rem
rem Edit variables below. Do NOT worry about a trailing backslash:
rem command-line calls use "\." to avoid quoted-path issues.
rem ============================================================

rem ---- User settings -------------------------------------------------------
set "FB2_INPUT_DIR=D:\fb2_test_suite"
set "TEST_OUTPUT_ROOT=D:\fb2_large_test_out"
set "ARCHIVE_PATH=D:\fb2_large_test_problems.zip"

set "EPUBCHECK_JAR=D:\Tools\epubcheck.jar"
set "RUN_EPUBCHECK=1"

rem EPUB versions to produce.
set "CONVERT_EPUB2=1"
set "CONVERT_EPUB3=1"

rem Large-run defaults.
rem NEWER_ONLY=1 allows safe resume after interruption.
set "NEWER_ONLY=1"
set "OVERWRITE=0"
set "FILENAME_MODE=source"
set "PROGRESS_EVERY=25"

rem Optional chunking for very large libraries.
rem START_INDEX is 1-based after sorting; MAX_FILES=0 means no limit.
set "START_INDEX=1"
set "MAX_FILES=0"

rem Packaging mode:
rem   problems = ZIP contains logs + only failed/EPUBCheck-problem files
rem   logs     = ZIP contains logs only
set "PACKAGE_MODE=problems"

set "BUILD_BEFORE_RUN=0"
rem -------------------------------------------------------------------------

set "BATCH_EXE=%~dp0out\Release\ExportEPUBBatch.exe"
set "EPUB2_DIR=%TEST_OUTPUT_ROOT%\epub2"
set "EPUB3_DIR=%TEST_OUTPUT_ROOT%\epub3"
set "LOG_DIR=%TEST_OUTPUT_ROOT%\logs"
set "STAGING_DIR=%TEST_OUTPUT_ROOT%\_package_for_chatgpt_large"

rem Safe values for command-line arguments.
set "FB2_INPUT_ARG=%FB2_INPUT_DIR%\."
set "EPUB2_ARG=%EPUB2_DIR%\."
set "EPUB3_ARG=%EPUB3_DIR%\."

set "EXPORT_FLAGS=--keep-folder-structure --recursive --filename %FILENAME_MODE% --progress-every %PROGRESS_EVERY%"
if "%NEWER_ONLY%"=="1" set "EXPORT_FLAGS=%EXPORT_FLAGS% --newer-only"
if "%OVERWRITE%"=="1" set "EXPORT_FLAGS=%EXPORT_FLAGS% --overwrite"
if not "%START_INDEX%"=="1" set "EXPORT_FLAGS=%EXPORT_FLAGS% --start-index %START_INDEX%"
if not "%MAX_FILES%"=="0" set "EXPORT_FLAGS=%EXPORT_FLAGS% --max-files %MAX_FILES%"

if "%BUILD_BEFORE_RUN%"=="1" (
  echo Building ExportEPUBBatch before large test run...
  set "NO_PAUSE=1"
  call "%~dp0BUILD_BATCH_WIN32_RELEASE.cmd"
  if errorlevel 1 goto :fail
)

if not exist "%FB2_INPUT_ARG%" (
  echo ERROR: FB2_INPUT_DIR does not exist:
  echo   "%FB2_INPUT_DIR%"
  goto :fail
)

if not exist "%BATCH_EXE%" (
  echo ERROR: ExportEPUBBatch.exe was not found:
  echo   "%BATCH_EXE%"
  echo Run BUILD_BATCH_WIN32_RELEASE.cmd first, or set BUILD_BEFORE_RUN=1.
  goto :fail
)

if not exist "%TEST_OUTPUT_ROOT%" mkdir "%TEST_OUTPUT_ROOT%" >nul 2>nul
if not exist "%EPUB2_DIR%" mkdir "%EPUB2_DIR%" >nul 2>nul
if not exist "%EPUB3_DIR%" mkdir "%EPUB3_DIR%" >nul 2>nul
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>nul

for /F %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "RUN_LOG=%LOG_DIR%\large_test_%STAMP%.log"

echo ============================================================
echo ExportEPUB large-volume test
echo Input:   "%FB2_INPUT_DIR%"
echo Output:  "%TEST_OUTPUT_ROOT%"
echo Archive: "%ARCHIVE_PATH%"
echo Flags:   %EXPORT_FLAGS%
echo ============================================================
> "%RUN_LOG%" echo ExportEPUB large-volume test started %DATE% %TIME%
>> "%RUN_LOG%" echo Input: "%FB2_INPUT_DIR%"
>> "%RUN_LOG%" echo Output: "%TEST_OUTPUT_ROOT%"
>> "%RUN_LOG%" echo Archive: "%ARCHIVE_PATH%"
>> "%RUN_LOG%" echo Flags: %EXPORT_FLAGS%

"%BATCH_EXE%" --version-info > "%LOG_DIR%\version_info.txt" 2>&1

if "%CONVERT_EPUB2%"=="1" (
  echo.
  echo Exporting EPUB 2. Console output is written to:
  echo   "%LOG_DIR%\export_epub2_console.log"
  "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB2_DIR%" --version 2 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub2.csv" > "%LOG_DIR%\export_epub2_console.log" 2>&1
  set "RC2=!ERRORLEVEL!"
  echo ExportEPUBBatch EPUB 2 exit code: !RC2!
  >> "%RUN_LOG%" echo ExportEPUBBatch EPUB 2 exit code: !RC2!
)

if "%CONVERT_EPUB3%"=="1" (
  echo.
  echo Exporting EPUB 3. Console output is written to:
  echo   "%LOG_DIR%\export_epub3_console.log"
  "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB3_DIR%" --version 3 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub3.csv" > "%LOG_DIR%\export_epub3_console.log" 2>&1
  set "RC3=!ERRORLEVEL!"
  echo ExportEPUBBatch EPUB 3 exit code: !RC3!
  >> "%RUN_LOG%" echo ExportEPUBBatch EPUB 3 exit code: !RC3!
)

if "%RUN_EPUBCHECK%"=="1" (
  if exist "%EPUBCHECK_JAR%" (
    echo.
    echo Running EPUBCheck. This may take a long time...
    java -version > "%LOG_DIR%\java_version.log" 2>&1
    if "%CONVERT_EPUB2%"=="1" (
      echo EPUBCheck EPUB 2 > "%LOG_DIR%\epubcheck_epub2.log"
      for /R "%EPUB2_DIR%" %%F in (*.epub) do (
        >> "%LOG_DIR%\epubcheck_epub2.log" echo.
        >> "%LOG_DIR%\epubcheck_epub2.log" echo ==== %%F ====
        java -jar "%EPUBCHECK_JAR%" "%%F" >> "%LOG_DIR%\epubcheck_epub2.log" 2>&1
      )
    )
    if "%CONVERT_EPUB3%"=="1" (
      echo EPUBCheck EPUB 3 > "%LOG_DIR%\epubcheck_epub3.log"
      for /R "%EPUB3_DIR%" %%F in (*.epub) do (
        >> "%LOG_DIR%\epubcheck_epub3.log" echo.
        >> "%LOG_DIR%\epubcheck_epub3.log" echo ==== %%F ====
        java -jar "%EPUBCHECK_JAR%" "%%F" >> "%LOG_DIR%\epubcheck_epub3.log" 2>&1
      )
    )
  ) else (
    echo EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%"
    echo EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%" > "%LOG_DIR%\epubcheck_skipped.txt"
  )
) else (
  echo EPUBCheck skipped: RUN_EPUBCHECK=0 > "%LOG_DIR%\epubcheck_skipped.txt"
)

call :make_package
if errorlevel 1 goto :fail

echo.
echo Done.
echo Send this archive for analysis:
echo   "%ARCHIVE_PATH%"
call :maybe_pause
exit /b 0

:make_package
echo.
echo Preparing compact package: %PACKAGE_MODE%
if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%" >nul 2>nul
mkdir "%STAGING_DIR%\fb2" >nul 2>nul
mkdir "%STAGING_DIR%\epub2" >nul 2>nul
mkdir "%STAGING_DIR%\epub3" >nul 2>nul
mkdir "%STAGING_DIR%\logs" >nul 2>nul

robocopy "%LOG_DIR%" "%STAGING_DIR%\logs" *.log *.csv *.txt /S /R:1 /W:1 >nul
if errorlevel 8 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; ^
  $fb2='%FB2_INPUT_DIR%'; $epub2='%EPUB2_DIR%'; $epub3='%EPUB3_DIR%'; $log='%LOG_DIR%'; $stage='%STAGING_DIR%'; $archive='%ARCHIVE_PATH%'; $mode='%PACKAGE_MODE%'; ^
  function Full([string]$p){ [IO.Path]::GetFullPath($p) } ^
  function Rel([string]$root,[string]$path){ $r=(Full $root).TrimEnd('\','/')+'\'; $p=Full $path; if($p.StartsWith($r,[StringComparison]::OrdinalIgnoreCase)){ return $p.Substring($r.Length) } return [IO.Path]::GetFileName($p) } ^
  function CopyRel([string]$src,[string]$root,[string]$dstRoot){ if([string]::IsNullOrWhiteSpace($src)){return}; if(!(Test-Path -LiteralPath $src)){return}; $rel=Rel $root $src; $dst=Join-Path $dstRoot $rel; New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null; Copy-Item -LiteralPath $src -Destination $dst -Force } ^
  $problemFb2=@{}; $problemEpub2=@{}; $problemEpub3=@{}; ^
  foreach($csvName in @('export_epub2.csv','export_epub3.csv')){ $csv=Join-Path $log $csvName; if(Test-Path -LiteralPath $csv){ Import-Csv -LiteralPath $csv -Delimiter ';' | ForEach-Object { if($_.Status -eq 'FAIL'){ if($_.Input){$problemFb2[$_.Input]=$true}; if($_.Version -eq '2' -and $_.Output){$problemEpub2[$_.Output]=$true}; if($_.Version -eq '3' -and $_.Output){$problemEpub3[$_.Output]=$true} } } } } ^
  function AddSourceFromEpub([string]$epub,[string]$root){ if([string]::IsNullOrWhiteSpace($epub)){return}; $rel=Rel $root $epub; $fb2rel=[IO.Path]::ChangeExtension($rel,'.fb2'); $src=Join-Path $fb2 $fb2rel; if(Test-Path -LiteralPath $src){$script:problemFb2[$src]=$true} } ^
  function ScanEpubCheck([string]$path,[string]$root,[hashtable]$dict){ if(!(Test-Path -LiteralPath $path)){return}; $current=$null; Get-Content -LiteralPath $path -Encoding UTF8 | ForEach-Object { if($_ -match '^==== (.*) ====$'){ $current=$Matches[1] } elseif($current -and ($_ -match '(ERROR|WARNING|FATAL)\(' -or $_ -match 'Exception|Traceback')){ $dict[$current]=$true; AddSourceFromEpub $current $root } } } ^
  ScanEpubCheck (Join-Path $log 'epubcheck_epub2.log') $epub2 $problemEpub2; ScanEpubCheck (Join-Path $log 'epubcheck_epub3.log') $epub3 $problemEpub3; ^
  if($mode -eq 'problems'){ foreach($p in $problemFb2.Keys){CopyRel $p $fb2 (Join-Path $stage 'fb2')}; foreach($p in $problemEpub2.Keys){CopyRel $p $epub2 (Join-Path $stage 'epub2')}; foreach($p in $problemEpub3.Keys){CopyRel $p $epub3 (Join-Path $stage 'epub3')} } ^
  $summary=Join-Path $stage 'notes.txt'; @('ExportEPUB large-volume test package','',('Created: '+(Get-Date)),('Source FB2 folder: '+$fb2),('Output folder: %TEST_OUTPUT_ROOT%'),('Package mode: '+$mode),'',('Problem FB2 files: '+$problemFb2.Count),('Problem EPUB 2 files: '+$problemEpub2.Count),('Problem EPUB 3 files: '+$problemEpub3.Count),'','If all counters are zero, send this logs-only archive for confirmation.') | Set-Content -LiteralPath $summary -Encoding UTF8; ^
  tree /F $stage | Out-File -FilePath (Join-Path $stage 'logs\tree.txt') -Encoding UTF8; if(Test-Path -LiteralPath $archive){Remove-Item -LiteralPath $archive -Force}; Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force"
if errorlevel 1 exit /b 1
exit /b 0

:fail
echo.
echo FAILED.
call :maybe_pause
exit /b 1

:maybe_pause
if /i "%NO_PAUSE%"=="1" exit /b 0
pause
exit /b 0
