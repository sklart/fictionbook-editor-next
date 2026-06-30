@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem ============================================================
rem ExportEPUB automated test conversion and package preparation
rem ============================================================
rem 1) Set FB2_INPUT_DIR below. Do NOT add a trailing backslash.
rem    Correct: set "FB2_INPUT_DIR=D:\Books"
rem    Wrong:   set "FB2_INPUT_DIR=D:\Books\"
rem    The script also protects command-line calls by using "\.".
rem 2) Build ExportEPUBBatch.exe if needed.
rem 3) Run this file.
rem 4) Send the created ZIP archive for analysis.
rem ============================================================

rem ---- User settings -------------------------------------------------------
set "FB2_INPUT_DIR=D:\fb2_test_suite"
set "TEST_OUTPUT_ROOT=D:\fb2_test_suite_out"
set "ARCHIVE_PATH=D:\fb2_epub_test_result.zip"

rem Optional. If the file exists and RUN_EPUBCHECK=1, EPUBCheck logs are added.
set "EPUBCHECK_JAR=D:\Tools\epubcheck.jar"
set "RUN_EPUBCHECK=1"

rem Usually leave these values as is.
set "CONVERT_EPUB2=1"
set "CONVERT_EPUB3=1"
set "RECURSIVE=1"
set "OVERWRITE=1"
set "BUILD_BEFORE_RUN=0"
rem -------------------------------------------------------------------------

rem Remove a trailing slash/backslash from directory variables where safe.
rem This avoids the Windows argv issue where "C:\Path\" can escape the closing quote.
call :trim_trailing_slash FB2_INPUT_DIR
call :trim_trailing_slash TEST_OUTPUT_ROOT

set "BATCH_EXE=%~dp0out\Release\ExportEPUBBatch.exe"
set "EPUB2_DIR=%TEST_OUTPUT_ROOT%\epub2"
set "EPUB3_DIR=%TEST_OUTPUT_ROOT%\epub3"
set "LOG_DIR=%TEST_OUTPUT_ROOT%\logs"
set "STAGING_DIR=%TEST_OUTPUT_ROOT%\_package_for_chatgpt"

rem Safe values for command-line arguments and robocopy sources.
rem Ending with . prevents a trailing backslash from escaping the closing quote.
set "FB2_INPUT_ARG=%FB2_INPUT_DIR%\."
set "EPUB2_ARG=%EPUB2_DIR%\."
set "EPUB3_ARG=%EPUB3_DIR%\."

set "EXPORT_FLAGS=--keep-folder-structure"
if "%RECURSIVE%"=="1" set "EXPORT_FLAGS=%EXPORT_FLAGS% --recursive"
if "%OVERWRITE%"=="1" set "EXPORT_FLAGS=%EXPORT_FLAGS% --overwrite"

if "%BUILD_BEFORE_RUN%"=="1" (
  echo Building ExportEPUBBatch before test run...
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
  echo.
  echo Run BUILD_BATCH_WIN32_RELEASE.cmd first, or set BUILD_BEFORE_RUN=1 in this file.
  goto :fail
)

if not exist "%TEST_OUTPUT_ROOT%" mkdir "%TEST_OUTPUT_ROOT%" >nul 2>nul
if not exist "%EPUB2_DIR%" mkdir "%EPUB2_DIR%" >nul 2>nul
if not exist "%EPUB3_DIR%" mkdir "%EPUB3_DIR%" >nul 2>nul
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>nul

for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "RUN_LOG=%LOG_DIR%\test_export_and_pack_%STAMP%.log"

call :log ============================================================
call :log ExportEPUB test conversion and package preparation
call :log Project: "%~dp0"
call :log Input:   "%FB2_INPUT_DIR%"
call :log Output:  "%TEST_OUTPUT_ROOT%"
call :log Archive: "%ARCHIVE_PATH%"
call :log ============================================================

"%BATCH_EXE%" --version-info > "%LOG_DIR%\version_info.txt" 2>&1

if "%CONVERT_EPUB2%"=="1" (
  call :blank
  call :log Exporting EPUB 2...
  "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB2_DIR%" --version 2 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub2.csv" > "%LOG_DIR%\export_epub2_console.log" 2>&1
  set "RC=!ERRORLEVEL!"
  type "%LOG_DIR%\export_epub2_console.log"
  call :log ExportEPUBBatch EPUB 2 exit code: !RC!
)

if "%CONVERT_EPUB3%"=="1" (
  call :blank
  call :log Exporting EPUB 3...
  "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB3_DIR%" --version 3 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub3.csv" > "%LOG_DIR%\export_epub3_console.log" 2>&1
  set "RC=!ERRORLEVEL!"
  type "%LOG_DIR%\export_epub3_console.log"
  call :log ExportEPUBBatch EPUB 3 exit code: !RC!
)

if "%RUN_EPUBCHECK%"=="1" (
  if exist "%EPUBCHECK_JAR%" (
    call :blank
    call :log Running EPUBCheck...
    java -version > "%LOG_DIR%\java_version.log" 2>&1

    if "%CONVERT_EPUB2%"=="1" (
      echo EPUBCheck EPUB 2 > "%LOG_DIR%\epubcheck_epub2.log"
      for /r "%EPUB2_ARG%" %%F in (*.epub) do (
        echo.>> "%LOG_DIR%\epubcheck_epub2.log"
        echo ==== %%F ====>> "%LOG_DIR%\epubcheck_epub2.log"
        java -jar "%EPUBCHECK_JAR%" "%%F" >> "%LOG_DIR%\epubcheck_epub2.log" 2>&1
      )
    )

    if "%CONVERT_EPUB3%"=="1" (
      echo EPUBCheck EPUB 3 > "%LOG_DIR%\epubcheck_epub3.log"
      for /r "%EPUB3_ARG%" %%F in (*.epub) do (
        echo.>> "%LOG_DIR%\epubcheck_epub3.log"
        echo ==== %%F ====>> "%LOG_DIR%\epubcheck_epub3.log"
        java -jar "%EPUBCHECK_JAR%" "%%F" >> "%LOG_DIR%\epubcheck_epub3.log" 2>&1
      )
    )
  ) else (
    call :log EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%"
    echo EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%" > "%LOG_DIR%\epubcheck_skipped.txt"
  )
) else (
  call :log EPUBCheck skipped: RUN_EPUBCHECK=0
  echo EPUBCheck skipped: RUN_EPUBCHECK=0 > "%LOG_DIR%\epubcheck_skipped.txt"
)

call :blank
call :log Preparing package folder...
if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%" >nul 2>nul
mkdir "%STAGING_DIR%\fb2" >nul 2>nul
mkdir "%STAGING_DIR%\epub2" >nul 2>nul
mkdir "%STAGING_DIR%\epub3" >nul 2>nul
mkdir "%STAGING_DIR%\logs" >nul 2>nul

robocopy "%FB2_INPUT_ARG%" "%STAGING_DIR%\fb2" *.fb2 /S /R:1 /W:1 > "%LOG_DIR%\copy_fb2.log"
if errorlevel 8 goto :copy_fail

if exist "%EPUB2_DIR%" (
  robocopy "%EPUB2_ARG%" "%STAGING_DIR%\epub2" *.epub /S /R:1 /W:1 > "%LOG_DIR%\copy_epub2.log"
  if errorlevel 8 goto :copy_fail
)

if exist "%EPUB3_DIR%" (
  robocopy "%EPUB3_ARG%" "%STAGING_DIR%\epub3" *.epub /S /R:1 /W:1 > "%LOG_DIR%\copy_epub3.log"
  if errorlevel 8 goto :copy_fail
)

robocopy "%LOG_DIR%" "%STAGING_DIR%\logs" *.log *.csv *.txt /S /R:1 /W:1 >nul
if errorlevel 8 goto :copy_fail

(
  echo ExportEPUB test package
  echo.
  echo Created: %DATE% %TIME%
  echo Project folder: %~dp0
  echo Source FB2 folder: %FB2_INPUT_DIR%
  echo Output folder: %TEST_OUTPUT_ROOT%
  echo Archive: %ARCHIVE_PATH%
  echo.
  echo Commands:
  if "%CONVERT_EPUB2%"=="1" echo "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB2_DIR%" --version 2 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub2.csv"
  if "%CONVERT_EPUB3%"=="1" echo "%BATCH_EXE%" --input "%FB2_INPUT_ARG%" --output "%EPUB3_DIR%" --version 3 %EXPORT_FLAGS% --report-csv "%LOG_DIR%\export_epub3.csv"
  echo.
  echo Expected negative tests may fail without creating EPUB files. This is normal for intentionally broken FB2/XML inputs.
) > "%STAGING_DIR%\notes.txt"

tree /F "%STAGING_DIR%" > "%STAGING_DIR%\logs\tree.txt"

call :blank
call :log Creating ZIP archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; if(Test-Path -LiteralPath '%ARCHIVE_PATH%'){Remove-Item -LiteralPath '%ARCHIVE_PATH%' -Force}; Compress-Archive -Path '%STAGING_DIR%\*' -DestinationPath '%ARCHIVE_PATH%' -Force"
if errorlevel 1 goto :fail

call :blank
call :log Done.
call :log Send this archive for analysis:
call :log   "%ARCHIVE_PATH%"
call :maybe_pause
exit /b 0

:trim_trailing_slash
set "_varname=%~1"
set "_value=!%_varname%!"
if not defined _value exit /b 0
:trim_loop
if "!_value:~-1!"=="\" (
  if not "!_value:~-2!"==":" (
    set "_value=!_value:~0,-1!"
    goto :trim_loop
  )
)
if "!_value:~-1!"=="/" (
  set "_value=!_value:~0,-1!"
  goto :trim_loop
)
set "%_varname%=%_value%"
exit /b 0

:copy_fail
echo ERROR: robocopy failed. See logs in "%LOG_DIR%".
goto :fail

:fail
echo.
echo FAILED.
call :maybe_pause
exit /b 1

:log
echo %*
>> "%RUN_LOG%" echo %*
exit /b 0

:blank
echo.
>> "%RUN_LOG%" echo.
exit /b 0

:maybe_pause
if /i "%NO_PAUSE%"=="1" exit /b 0
pause
exit /b 0
