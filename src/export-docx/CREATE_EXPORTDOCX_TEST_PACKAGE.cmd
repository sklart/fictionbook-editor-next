@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem ================================================================
rem ExportDOCX test package creator
rem ================================================================
rem Edit only the variables in this block before running the script.
rem The script converts FB2 files from SOURCE_DIR to DOCX and then
rem creates a ZIP package with sources, converted DOCX files and logs.
rem ================================================================

set "SOURCE_DIR=D:\fb2_test_suite"
set "OUTPUT_DIR=D:\fb2_test_suite_docx"
set "PACKAGE_ZIP=D:\fb2_test_suite_exportdocx_test_package.zip"

rem 1 = search FB2 files in subfolders, 0 = only SOURCE_DIR
set "RECURSIVE=1"

rem 1 = delete OUTPUT_DIR before conversion, 0 = keep previous results
set "CLEAN_OUTPUT=1"

rem 1 = also unpack *.fb2.zip from SOURCE_DIR to a temporary staging folder
rem     and include the unpacked FB2 files in conversion.
set "INCLUDE_FB2_ZIP=1"

rem 1 = return ExportDOCXBatch exit code.
rem 0 = always return 0 if the ZIP package was created. This is convenient
rem     for suites that intentionally contain negative/broken FB2 files.
set "EXIT_WITH_EXPORT_CODE=0"

rem Additional arguments for ExportDOCXBatch.exe.
rem Recommended for testing: retry transient failures, keep original dates,
rem print regular progress and continue after failed/negative test files.
set "BATCH_ARGS=-Retries 2 -PreserveDates -ProgressEvery 1 -ValidateDocx"

rem ================================================================
rem Do not edit below this line unless you want to change the workflow.
rem ================================================================

set "SCRIPT_DIR=%~dp0"
set "BATCH_EXE=%SCRIPT_DIR%out\Release\ExportDOCXBatch.exe"
set "DLL_PATH=%SCRIPT_DIR%out\Release\ExportDOCX.dll"

if not exist "%BATCH_EXE%" set "BATCH_EXE=%SCRIPT_DIR%ExportDOCXBatch.exe"
if not exist "%DLL_PATH%" set "DLL_PATH=%SCRIPT_DIR%ExportDOCX.dll"

if not exist "%BATCH_EXE%" (
    echo ERROR: ExportDOCXBatch.exe not found.
    echo Build the project first: BUILD_WIN32_RELEASE.cmd
    exit /b 2
)

if not exist "%DLL_PATH%" (
    echo ERROR: ExportDOCX.dll not found.
    echo Build the project first: BUILD_WIN32_RELEASE.cmd
    exit /b 2
)

if not exist "%SOURCE_DIR%" (
    echo ERROR: SOURCE_DIR does not exist: %SOURCE_DIR%
    exit /b 2
)

for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
set "PACKAGE_ROOT=%TEMP%\ExportDOCX_test_package_%STAMP%"
set "STAGING_SOURCE=%TEMP%\ExportDOCX_test_input_%STAMP%"

if "%CLEAN_OUTPUT%"=="1" (
    if exist "%OUTPUT_DIR%" (
        echo Removing old output directory: %OUTPUT_DIR%
        rmdir /s /q "%OUTPUT_DIR%"
    )
)

if exist "%PACKAGE_ROOT%" rmdir /s /q "%PACKAGE_ROOT%"
if exist "%STAGING_SOURCE%" rmdir /s /q "%STAGING_SOURCE%"

mkdir "%OUTPUT_DIR%" >nul 2>nul
mkdir "%PACKAGE_ROOT%\source_fb2" >nul 2>nul
mkdir "%PACKAGE_ROOT%\converted_docx" >nul 2>nul
mkdir "%PACKAGE_ROOT%\logs" >nul 2>nul
mkdir "%STAGING_SOURCE%" >nul 2>nul

set "RECURSE_ARG="
if "%RECURSIVE%"=="1" set "RECURSE_ARG=-Recurse"

set "CSV_LOG=%OUTPUT_DIR%\ExportDOCX_batch_log.csv"
set "HTML_REPORT=%OUTPUT_DIR%\report.html"
set "SUMMARY_TXT=%OUTPUT_DIR%\summary.txt"
set "FAILED_TXT=%OUTPUT_DIR%\failed.txt"
set "OK_TXT=%OUTPUT_DIR%\ok.txt"
set "CONSOLE_LOG=%OUTPUT_DIR%\ExportDOCX_batch_console.log"

echo ================================================================
echo ExportDOCX test conversion
echo ================================================================
echo Source:  %SOURCE_DIR%
echo Output:  %OUTPUT_DIR%
echo Package: %PACKAGE_ZIP%
echo.

echo Preparing staging input...
robocopy "%SOURCE_DIR%" "%STAGING_SOURCE%" *.fb2 /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

if "%INCLUDE_FB2_ZIP%"=="1" (
    echo Unpacking *.fb2.zip files for conversion...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "$src='%SOURCE_DIR%';" ^
        "$stage='%STAGING_SOURCE%';" ^
        "$count=0;" ^
        "Get-ChildItem -LiteralPath $src -Recurse -File -Filter '*.fb2.zip' | ForEach-Object {" ^
        "  $rel=$_.FullName.Substring($src.Length).TrimStart('\','/');" ^
        "  $relDir=Split-Path -Path $rel -Parent;" ^
        "  $targetDir=Join-Path $stage $relDir;" ^
        "  $tmp=Join-Path $env:TEMP ('ExportDOCX_fb2zip_' + [guid]::NewGuid().ToString('N'));" ^
        "  New-Item -ItemType Directory -Force -Path $tmp | Out-Null;" ^
        "  try {" ^
        "    Expand-Archive -LiteralPath $_.FullName -DestinationPath $tmp -Force;" ^
        "    $fb2=Get-ChildItem -LiteralPath $tmp -Recurse -File -Filter '*.fb2' | Select-Object -First 1;" ^
        "    if($fb2) {" ^
        "      New-Item -ItemType Directory -Force -Path $targetDir | Out-Null;" ^
        "      $outName=[IO.Path]::GetFileNameWithoutExtension($_.Name);" ^
        "      if(-not $outName.ToLowerInvariant().EndsWith('.fb2')) { $outName=[IO.Path]::GetFileNameWithoutExtension($outName)+'.fb2'; }" ^
        "      Copy-Item -LiteralPath $fb2.FullName -Destination (Join-Path $targetDir $outName) -Force;" ^
        "      $count++;" ^
        "    }" ^
        "  } finally {" ^
        "    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue;" ^
        "  }" ^
        "};" ^
        "Write-Host ('Unpacked FB2 ZIP files: ' + $count)"
)

echo Running ExportDOCXBatch.exe...
"%BATCH_EXE%" "%STAGING_SOURCE%" "%OUTPUT_DIR%" %RECURSE_ARG% %BATCH_ARGS% -Dll "%DLL_PATH%" -Log "%CSV_LOG%" -HtmlReport "%HTML_REPORT%" -Summary "%SUMMARY_TXT%" -FailedList "%FAILED_TXT%" -OkList "%OK_TXT%" -PrintFailed > "%CONSOLE_LOG%" 2>&1
set "EXPORT_EXIT=%ERRORLEVEL%"

rem Make sure the console log is UTF-8 even if the console runtime wrote UTF-16LE.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$p='%CONSOLE_LOG%';" ^
    "if(Test-Path -LiteralPath $p) {" ^
    "  $b=[IO.File]::ReadAllBytes($p);" ^
    "  if($b.Length -ge 4 -and $b[1] -eq 0 -and $b[3] -eq 0) {" ^
    "    $s=[Text.Encoding]::Unicode.GetString($b);" ^
    "    [IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($true));" ^
    "  }" ^
    "}"

echo.
echo Batch exit code: %EXPORT_EXIT%
echo Batch console log:
echo %CONSOLE_LOG%
echo.
type "%CONSOLE_LOG%"
echo.

echo Creating package folder...
(
    echo ExportDOCX test package
    echo Created: %DATE% %TIME%
    echo.
    echo SOURCE_DIR=%SOURCE_DIR%
    echo STAGING_SOURCE=%STAGING_SOURCE%
    echo OUTPUT_DIR=%OUTPUT_DIR%
    echo PACKAGE_ZIP=%PACKAGE_ZIP%
    echo RECURSIVE=%RECURSIVE%
    echo CLEAN_OUTPUT=%CLEAN_OUTPUT%
    echo INCLUDE_FB2_ZIP=%INCLUDE_FB2_ZIP%
    echo BATCH_ARGS=%BATCH_ARGS%
    echo EXPORT_EXIT=%EXPORT_EXIT%
    echo EXIT_WITH_EXPORT_CODE=%EXIT_WITH_EXPORT_CODE%
    echo.
    echo BATCH_EXE=%BATCH_EXE%
    echo DLL_PATH=%DLL_PATH%
) > "%PACKAGE_ROOT%\logs\test_info.txt"

dir /s /b "%SOURCE_DIR%" > "%PACKAGE_ROOT%\logs\source_tree.txt" 2>nul
dir /s /b "%OUTPUT_DIR%" > "%PACKAGE_ROOT%\logs\output_tree.txt" 2>nul
dir /s /b "%STAGING_SOURCE%" > "%PACKAGE_ROOT%\logs\staging_source_tree.txt" 2>nul

rem Copy original FB2 files and FB2 ZIP archives, preserving the folder tree.
robocopy "%SOURCE_DIR%" "%PACKAGE_ROOT%\source_fb2" *.fb2 *.fb2.zip /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

rem Copy converted DOCX files, preserving the folder tree.
robocopy "%OUTPUT_DIR%" "%PACKAGE_ROOT%\converted_docx" *.docx /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

rem Copy logs and reports, including per-book *_export_report.txt files.
robocopy "%OUTPUT_DIR%" "%PACKAGE_ROOT%\logs" *.txt *.csv *.html *.log /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

echo Creating ZIP archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$zip='%PACKAGE_ZIP%'; $pkg='%PACKAGE_ROOT%'; if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip -Force}; Compress-Archive -Path (Join-Path $pkg '*') -DestinationPath $zip -Force"
set "ZIP_EXIT=%ERRORLEVEL%"

if "%ZIP_EXIT%"=="0" (
    echo.
    echo DONE.
    echo Package created:
    echo %PACKAGE_ZIP%
) else (
    echo.
    echo ERROR: ZIP package was not created.
    echo Temporary package folder:
    echo %PACKAGE_ROOT%
    exit /b %ZIP_EXIT%
)

rem Remove temporary staging input. Keep PACKAGE_ROOT only if ZIP creation failed.
if exist "%STAGING_SOURCE%" rmdir /s /q "%STAGING_SOURCE%" >nul 2>nul

echo.
echo Send this ZIP package for analysis together with a short note about visible problems, if any.

if "%EXIT_WITH_EXPORT_CODE%"=="1" exit /b %EXPORT_EXIT%
exit /b 0
