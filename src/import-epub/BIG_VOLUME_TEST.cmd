@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem ============================================================================
rem ImportEPUB large volume test runner
rem ============================================================================
rem Designed for large EPUB collections, for example 3178 files / 4.28 GB.
rem It runs ImportEPUBBatch with report flushing enabled, creates CSV/HTML reports,
rem writes a console log, and packages reports + problematic files for analysis.
rem ============================================================================

rem Folder with source EPUB files.
set "INPUT_DIR=D:\epub_big_suite"

rem Folder for converted FB2 files.
set "OUTPUT_DIR=D:\epub_big_suite_fb2"

rem Folder for final ZIP packages.
set "PACKAGE_DIR=D:\epub_big_test_packages"

rem Large-test options. --flush-report-each protects the report if the long run is interrupted.
set "IMPORT_OPTIONS=--recursive --preserve-tree --stats --log --svg png --flush-report-each --quiet --isolate-crashes"

rem 1 = resume mode: skip FB2 files already created earlier.
rem 0 = overwrite mode: convert everything again.
set "RESUME_MODE=1"

rem Optional smoke limit. 0 = process all files.
set "MAX_FILES=0"

rem 1 = build ImportEPUB.dll/ImportEPUBBatch.exe before test.
set "BUILD_BEFORE_RUN=0"

rem 1 = build LunaSVG static libs and ImportEPUBLunaSVG.dll before test.
set "BUILD_LUNASVG_BEFORE_RUN=0"

rem 1 = run CHECK_DEPENDENCIES.cmd before test.
set "CHECK_DEPENDENCIES_BEFORE_RUN=1"

rem Package only FAILED / OK_WITH_WARNINGS rows. This is recommended for big tests.
set "PACKAGE_PROBLEMS=1"

rem Limit number of problem rows copied into the ZIP, to avoid huge packages.
set "MAX_PROBLEM_ROWS=250"

set "SCRIPT_DIR=%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%BIG_VOLUME_TEST.ps1" ^
  -InputDir "%INPUT_DIR%" ^
  -OutputDir "%OUTPUT_DIR%" ^
  -PackageDir "%PACKAGE_DIR%" ^
  -ImportOptions "%IMPORT_OPTIONS%" ^
  -ResumeMode %RESUME_MODE% ^
  -MaxFiles %MAX_FILES% ^
  -BuildBeforeRun %BUILD_BEFORE_RUN% ^
  -BuildLunaSvgBeforeRun %BUILD_LUNASVG_BEFORE_RUN% ^
  -CheckDependenciesBeforeRun %CHECK_DEPENDENCIES_BEFORE_RUN% ^
  -PackageProblems %PACKAGE_PROBLEMS% ^
  -MaxProblemRows %MAX_PROBLEM_ROWS%

exit /b %ERRORLEVEL%
