@echo off
setlocal EnableExtensions
chcp 65001 >nul

rem ============================================================================
rem Packs only problematic ImportEPUB batch results for analysis.
rem Edit paths below if your test folders are different.
rem ============================================================================

set "INPUT_DIR=D:\epub_test_suite"
set "OUTPUT_DIR=D:\epub_test_suite_fb2"
set "REPORT_CSV=%OUTPUT_DIR%\ImportEPUBBatch_report.csv"
set "PACKAGE_DIR=D:\epub_import_test_packages"

rem Include rows by status.
set "PACK_FAILED=1"
set "PACK_WARNINGS=1"
set "PACK_SKIPPED=0"

set "SCRIPT_DIR=%~dp0"
set "OUT_DIR=%SCRIPT_DIR%out\Release"
if not exist "%OUT_DIR%" set "OUT_DIR=%SCRIPT_DIR%out"

if not exist "%REPORT_CSV%" (
  echo ERROR: report CSV not found:
  echo   "%REPORT_CSV%"
  exit /b 1
)

if not exist "%PACKAGE_DIR%" mkdir "%PACKAGE_DIR%"
for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"

set "PACKAGE_WORK=%PACKAGE_DIR%\ImportEPUB_problem_files_%STAMP%"
set "PACKAGE_ZIP=%PACKAGE_DIR%\ImportEPUB_problem_files_%STAMP%.zip"
set "PS1=%TEMP%\ImportEPUB_pack_problem_files_%STAMP%.ps1"

> "%PS1%" (
  echo $ErrorActionPreference = 'Stop'
  echo $inputDir = '%INPUT_DIR%'
  echo $outputDir = '%OUTPUT_DIR%'
  echo $reportCsv = '%REPORT_CSV%'
  echo $packageWork = '%PACKAGE_WORK%'
  echo $packageZip = '%PACKAGE_ZIP%'
  echo $scriptDir = '%SCRIPT_DIR%'
  echo $outDir = '%OUT_DIR%'
  echo $packFailed = '%PACK_FAILED%' -eq '1'
  echo $packWarnings = '%PACK_WARNINGS%' -eq '1'
  echo $packSkipped = '%PACK_SKIPPED%' -eq '1'
  echo function New-Dir($p^) { if (-not (Test-Path -LiteralPath $p^)^) { New-Item -ItemType Directory -Path $p ^| Out-Null } }
  echo function Copy-KeepRelative($file, $base, $dstRoot^) {
  echo   if ([string]::IsNullOrWhiteSpace($file^) -or -not (Test-Path -LiteralPath $file^)^) { return }
  echo   $full = [IO.Path]::GetFullPath($file^)
  echo   $baseFull = [IO.Path]::GetFullPath($base^).TrimEnd('\','/'^) + '\'
  echo   if ($full.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase^)^) { $rel = $full.Substring($baseFull.Length^) } else { $rel = Split-Path -Leaf $full }
  echo   $dst = Join-Path $dstRoot $rel
  echo   New-Dir (Split-Path -Parent $dst^)
  echo   Copy-Item -LiteralPath $full -Destination $dst -Force
  echo }
  echo if (Test-Path -LiteralPath $packageWork^) { Remove-Item -LiteralPath $packageWork -Recurse -Force }
  echo New-Dir $packageWork
  echo New-Dir (Join-Path $packageWork 'report'^)
  echo New-Dir (Join-Path $packageWork 'source_epub'^)
  echo New-Dir (Join-Path $packageWork 'converted_fb2'^)
  echo New-Dir (Join-Path $packageWork 'logs'^)
  echo New-Dir (Join-Path $packageWork 'binaries'^)
  echo $rows = Import-Csv -LiteralPath $reportCsv -Delimiter ';'
  echo $selected = @(^)
  echo foreach ($r in $rows^) {
  echo   $st = [string]$r.Status
  echo   if (($packFailed -and $st -eq 'FAILED'^) -or ($packWarnings -and $st -like '*WARNING*'^) -or ($packSkipped -and $st -eq 'SKIPPED'^)^) { $selected += $r }
  echo }
  echo Copy-Item -LiteralPath $reportCsv -Destination (Join-Path $packageWork 'report'^) -Force
  echo $reportHtml = [IO.Path]::ChangeExtension($reportCsv, '.html'^)
  echo if (Test-Path -LiteralPath $reportHtml^) { Copy-Item -LiteralPath $reportHtml -Destination (Join-Path $packageWork 'report'^) -Force }
  echo foreach ($r in $selected^) {
  echo   Copy-KeepRelative $r.Source $inputDir (Join-Path $packageWork 'source_epub'^)
  echo   Copy-KeepRelative $r.Output $outputDir (Join-Path $packageWork 'converted_fb2'^)
  echo   foreach ($f in @($r.Source, $r.Output^)^) {
  echo     if (-not [string]::IsNullOrWhiteSpace($f^)^) {
  echo       $dir = Split-Path -Parent $f
  echo       $stem = [IO.Path]::GetFileNameWithoutExtension($f^)
  echo       foreach ($log in @((Join-Path $dir ($stem + '.ImportEPUB.log'^)^), (Join-Path $dir ($stem + '.failed-import.log'^)^)^)^) {
  echo         if (Test-Path -LiteralPath $log^) { Copy-KeepRelative $log $dir (Join-Path $packageWork 'logs'^) }
  echo       }
  echo     }
  echo   }
  echo }
  echo foreach ($name in @('ImportEPUB.dll','ImportEPUBBatch.exe','ImportEPUBLunaSVG.dll'^)^) {
  echo   $p = Join-Path $outDir $name
  echo   if (Test-Path -LiteralPath $p^) { Copy-Item -LiteralPath $p -Destination (Join-Path $packageWork 'binaries'^) -Force }
  echo }
  echo $readme = @()
  echo $readme += 'ImportEPUB problem files package'
  echo $readme += ('Generated: ' + (Get-Date^)^)
  echo $readme += ('Source report: ' + $reportCsv^)
  echo $readme += ('Selected rows: ' + $selected.Count^)
  echo $readme += 'Includes FAILED and/or OK_WITH_WARNINGS rows depending on script variables.'
  echo $readme ^| Set-Content -LiteralPath (Join-Path $packageWork 'README_PROBLEM_PACKAGE.txt'^) -Encoding UTF8
  echo if (Test-Path -LiteralPath $packageZip^) { Remove-Item -LiteralPath $packageZip -Force }
  echo Compress-Archive -Path (Join-Path $packageWork '*'^) -DestinationPath $packageZip -Force
  echo Write-Host ('Problem files package: ' + $packageZip^)
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
set "RC=%ERRORLEVEL%"
del /f /q "%PS1%" >nul 2>nul
exit /b %RC%
