@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

rem ============================================================
rem ExportEPUB focused test conversion and package preparation
rem ============================================================
rem Converts only FB2 files listed in PROBLEM_FILES_ExportEPUB.txt,
rem runs EPUBCheck if available, and creates a small ZIP package.
rem
rem Edit FB2_INPUT_DIR below. Do not put a trailing backslash.
rem Correct:   set "FB2_INPUT_DIR=D:\fb2_test_suite"
rem Incorrect: set "FB2_INPUT_DIR=D:\fb2_test_suite\"
rem ============================================================

rem ---- User settings -------------------------------------------------------
set "FB2_INPUT_DIR=D:\fb2_test_suite"
set "TEST_OUTPUT_ROOT=D:\fb2_problem_cases_out"
set "ARCHIVE_PATH=D:\fb2_epub_problem_cases.zip"
set "PROBLEM_LIST=%~dp0PROBLEM_FILES_ExportEPUB.txt"

rem Optional. If the file exists and RUN_EPUBCHECK=1, EPUBCheck logs are added.
set "EPUBCHECK_JAR=D:\Tools\epubcheck.jar"
set "RUN_EPUBCHECK=1"

rem Usually leave these values as is.
set "BUILD_BEFORE_RUN=0"
set "OVERWRITE=1"
set "CLEAN_OUTPUT_BEFORE_RUN=1"
rem -------------------------------------------------------------------------

rem Normalize accidental trailing slash/backslash.
if "%FB2_INPUT_DIR:~-1%"=="\" set "FB2_INPUT_DIR=%FB2_INPUT_DIR:~0,-1%"
if "%FB2_INPUT_DIR:~-1%"=="/" set "FB2_INPUT_DIR=%FB2_INPUT_DIR:~0,-1%"

set "BATCH_EXE=%~dp0out\Release\ExportEPUBBatch.exe"
set "EPUB2_DIR=%TEST_OUTPUT_ROOT%\epub2"
set "EPUB3_DIR=%TEST_OUTPUT_ROOT%\epub3"
set "LOG_DIR=%TEST_OUTPUT_ROOT%\logs"
set "STAGING_DIR=%TEST_OUTPUT_ROOT%\_problem_cases_for_chatgpt"

if "%BUILD_BEFORE_RUN%"=="1" (
    echo Building ExportEPUBBatch before focused test run...
    set "NO_PAUSE=1"
    call "%~dp0BUILD_BATCH_WIN32_RELEASE.cmd"
    if errorlevel 1 goto :fail
)

if not exist "%FB2_INPUT_DIR%" (
    echo ERROR: FB2_INPUT_DIR does not exist:
    echo   "%FB2_INPUT_DIR%"
    goto :fail
)

if not exist "%PROBLEM_LIST%" (
    echo ERROR: PROBLEM_LIST does not exist:
    echo   "%PROBLEM_LIST%"
    goto :fail
)

if not exist "%BATCH_EXE%" (
    echo ERROR: ExportEPUBBatch.exe was not found:
    echo   "%BATCH_EXE%"
    echo.
    echo Run BUILD_BATCH_WIN32_RELEASE.cmd first, or set BUILD_BEFORE_RUN=1 in this file.
    goto :fail
)

if "%CLEAN_OUTPUT_BEFORE_RUN%"=="1" (
    if exist "%TEST_OUTPUT_ROOT%" rmdir /s /q "%TEST_OUTPUT_ROOT%"
)
if not exist "%TEST_OUTPUT_ROOT%" mkdir "%TEST_OUTPUT_ROOT%" >nul 2>nul
if not exist "%EPUB2_DIR%" mkdir "%EPUB2_DIR%" >nul 2>nul
if not exist "%EPUB3_DIR%" mkdir "%EPUB3_DIR%" >nul 2>nul
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>nul

for /F %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "RUN_LOG=%LOG_DIR%\test_export_problem_files_%STAMP%.log"
set "EPUB2_CONSOLE=%LOG_DIR%\export_problem_epub2_console.log"
set "EPUB3_CONSOLE=%LOG_DIR%\export_problem_epub3_console.log"
set "EPUB2_CSV=%LOG_DIR%\export_problem_epub2.csv"
set "EPUB3_CSV=%LOG_DIR%\export_problem_epub3.csv"
set "EPUB2_CHECK=%LOG_DIR%\epubcheck_problem_epub2.log"
set "EPUB3_CHECK=%LOG_DIR%\epubcheck_problem_epub3.log"
set "VERSION_INFO=%LOG_DIR%\version_info.txt"

call :log ============================================================
call :log ExportEPUB focused problem-file test
call :log Project: "%~dp0"
call :log Input:   "%FB2_INPUT_DIR%"
call :log List:    "%PROBLEM_LIST%"
call :log Output:  "%TEST_OUTPUT_ROOT%"
call :log Archive: "%ARCHIVE_PATH%"
call :log ============================================================

"%BATCH_EXE%" --version-info > "%VERSION_INFO%" 2>&1

call :blank
call :log Files selected by --from-list:
"%BATCH_EXE%" --input "%FB2_INPUT_DIR%" --from-list "%PROBLEM_LIST%" --check-input > "%LOG_DIR%\problem_files_resolved.txt" 2>&1
type "%LOG_DIR%\problem_files_resolved.txt"

call :blank
call :log Exporting selected files to EPUB 2...
"%BATCH_EXE%" ^
  --input "%FB2_INPUT_DIR%" ^
  --from-list "%PROBLEM_LIST%" ^
  --output "%EPUB2_DIR%" ^
  --version 2 ^
  --keep-folder-structure ^
  --overwrite ^
  --report-csv "%EPUB2_CSV%" ^
  > "%EPUB2_CONSOLE%" 2>&1
set "RC2=!ERRORLEVEL!"
type "%EPUB2_CONSOLE%"
call :log ExportEPUBBatch EPUB 2 exit code: !RC2!

call :blank
call :log Exporting selected files to EPUB 3...
"%BATCH_EXE%" ^
  --input "%FB2_INPUT_DIR%" ^
  --from-list "%PROBLEM_LIST%" ^
  --output "%EPUB3_DIR%" ^
  --version 3 ^
  --keep-folder-structure ^
  --overwrite ^
  --report-csv "%EPUB3_CSV%" ^
  > "%EPUB3_CONSOLE%" 2>&1
set "RC3=!ERRORLEVEL!"
type "%EPUB3_CONSOLE%"
call :log ExportEPUBBatch EPUB 3 exit code: !RC3!

if "%RUN_EPUBCHECK%"=="1" (
    if exist "%EPUBCHECK_JAR%" (
        call :blank
        call :log Running EPUBCheck for selected EPUB files...
        echo EPUBCheck selected EPUB 2 > "%EPUB2_CHECK%"
        for /R "%EPUB2_DIR%" %%F in (*.epub) do (
            echo.>>"%EPUB2_CHECK%"
            echo ==== %%F ====>>"%EPUB2_CHECK%"
            java -jar "%EPUBCHECK_JAR%" "%%F" >> "%EPUB2_CHECK%" 2>&1
        )
        echo EPUBCheck selected EPUB 3 > "%EPUB3_CHECK%"
        for /R "%EPUB3_DIR%" %%F in (*.epub) do (
            echo.>>"%EPUB3_CHECK%"
            echo ==== %%F ====>>"%EPUB3_CHECK%"
            java -jar "%EPUBCHECK_JAR%" "%%F" >> "%EPUB3_CHECK%" 2>&1
        )
    ) else (
        call :log EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%"
        echo EPUBCheck skipped: JAR file not found: "%EPUBCHECK_JAR%" > "%LOG_DIR%\epubcheck_problem_skipped.txt"
    )
) else (
    call :log EPUBCheck skipped: RUN_EPUBCHECK=0
    echo EPUBCheck skipped: RUN_EPUBCHECK=0 > "%LOG_DIR%\epubcheck_problem_skipped.txt"
)

call :blank
call :log Preparing small package folder...
if exist "%STAGING_DIR%" rmdir /s /q "%STAGING_DIR%"
mkdir "%STAGING_DIR%" >nul 2>nul
mkdir "%STAGING_DIR%\fb2" >nul 2>nul
mkdir "%STAGING_DIR%\epub2" >nul 2>nul
mkdir "%STAGING_DIR%\epub3" >nul 2>nul
mkdir "%STAGING_DIR%\logs" >nul 2>nul

for /F "usebackq delims=" %%R in ("%PROBLEM_LIST%") do call :copy_one "%%R"
copy /Y "%LOG_DIR%\*.log" "%STAGING_DIR%\logs\" >nul 2>nul
copy /Y "%LOG_DIR%\*.csv" "%STAGING_DIR%\logs\" >nul 2>nul
copy /Y "%LOG_DIR%\*.txt" "%STAGING_DIR%\logs\" >nul 2>nul
copy /Y "%PROBLEM_LIST%" "%STAGING_DIR%\logs\" >nul 2>nul

> "%STAGING_DIR%\notes.txt" echo ExportEPUB focused problem-file test package
>>"%STAGING_DIR%\notes.txt" echo Created: %DATE% %TIME%
>>"%STAGING_DIR%\notes.txt" echo Project folder: %~dp0
>>"%STAGING_DIR%\notes.txt" echo Source FB2 folder: %FB2_INPUT_DIR%
>>"%STAGING_DIR%\notes.txt" echo Problem list: %PROBLEM_LIST%
>>"%STAGING_DIR%\notes.txt" echo Output folder: %TEST_OUTPUT_ROOT%
>>"%STAGING_DIR%\notes.txt" echo Archive: %ARCHIVE_PATH%
>>"%STAGING_DIR%\notes.txt" echo.
>>"%STAGING_DIR%\notes.txt" echo Commands:
>>"%STAGING_DIR%\notes.txt" echo "%BATCH_EXE%" --input "%FB2_INPUT_DIR%" --from-list "%PROBLEM_LIST%" --output "%EPUB2_DIR%" --version 2 --keep-folder-structure --overwrite --report-csv "%EPUB2_CSV%"
>>"%STAGING_DIR%\notes.txt" echo "%BATCH_EXE%" --input "%FB2_INPUT_DIR%" --from-list "%PROBLEM_LIST%" --output "%EPUB3_DIR%" --version 3 --keep-folder-structure --overwrite --report-csv "%EPUB3_CSV%"

tree /F "%STAGING_DIR%" > "%LOG_DIR%\tree_problem_files.txt"
copy /Y "%LOG_DIR%\tree_problem_files.txt" "%STAGING_DIR%\logs\" >nul 2>nul

call :blank
call :log Creating ZIP archive...
if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" >nul 2>nul
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGING_DIR%\*' -DestinationPath '%ARCHIVE_PATH%' -Force"
if errorlevel 1 goto :fail

call :blank
call :log DONE.
call :log Archive created: "%ARCHIVE_PATH%"
if not "!RC2!"=="0" call :log WARNING: EPUB 2 export finished with non-zero exit code !RC2!.
if not "!RC3!"=="0" call :log WARNING: EPUB 3 export finished with non-zero exit code !RC3!.
goto :success

:copy_one
set "REL=%~1"
if "%REL%"=="" exit /b 0
if "%REL:~0,1%"=="#" exit /b 0
if "%REL:~0,1%"==";" exit /b 0
set "IN_PATH=%FB2_INPUT_DIR%\%REL%"
set "OUT_REL=%REL:.fb2=.epub%"
set "OUT2=%EPUB2_DIR%\%OUT_REL%"
set "OUT3=%EPUB3_DIR%\%OUT_REL%"
set "DST_FB2=%STAGING_DIR%\fb2\%REL%"
set "DST_EPUB2=%STAGING_DIR%\epub2\%OUT_REL%"
set "DST_EPUB3=%STAGING_DIR%\epub3\%OUT_REL%"

for %%P in ("%DST_FB2%") do if not exist "%%~dpP" mkdir "%%~dpP" >nul 2>nul
for %%P in ("%DST_EPUB2%") do if not exist "%%~dpP" mkdir "%%~dpP" >nul 2>nul
for %%P in ("%DST_EPUB3%") do if not exist "%%~dpP" mkdir "%%~dpP" >nul 2>nul

if exist "%IN_PATH%" copy /Y "%IN_PATH%" "%DST_FB2%" >nul 2>nul
if exist "%OUT2%" copy /Y "%OUT2%" "%DST_EPUB2%" >nul 2>nul
if exist "%OUT3%" copy /Y "%OUT3%" "%DST_EPUB3%" >nul 2>nul
exit /b 0

:log
echo %*
echo %*>>"%RUN_LOG%"
exit /b 0

:blank
echo.
echo.>>"%RUN_LOG%"
exit /b 0

:fail
echo.
echo FAILED.
call :maybe_pause
exit /b 1

:success
call :maybe_pause
exit /b 0

:maybe_pause
if /I "%NO_PAUSE%"=="1" exit /b 0
pause
exit /b 0
