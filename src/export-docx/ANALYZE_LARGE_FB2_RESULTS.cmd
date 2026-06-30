@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "LOG_DIR=%~1"
if "%LOG_DIR%"=="" set "LOG_DIR=%~dp0out\Release"

set "CSV_LOG=%LOG_DIR%\ExportDOCX_batch_log.csv"
set "SUMMARY_TXT=%LOG_DIR%\summary.txt"
set "ANALYSIS_TXT=%LOG_DIR%\large_test_analysis.txt"

if not exist "%CSV_LOG%" (
  echo CSV log not found: %CSV_LOG%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$csv='%CSV_LOG%'; $summary='%SUMMARY_TXT%'; $out='%ANALYSIS_TXT%';" ^
  "$rows=Import-Csv -LiteralPath $csv -Delimiter ';';" ^
  "$total=($rows|Measure-Object).Count;" ^
  "$groups=$rows|Group-Object Status|Sort-Object Name;" ^
  "$slow=$rows|Where-Object {$_.ElapsedMs -as [int]}|Sort-Object {[int]$_.ElapsedMs} -Descending|Select-Object -First 30;" ^
  "$bad=$rows|Where-Object {$_.Status -in @('FAIL','DOCX_INVALID')};" ^
  "$warn=$rows|Where-Object {$_.Status -eq 'OK_WITH_WARNINGS'};" ^
  "$lines=New-Object System.Collections.Generic.List[string];" ^
  "$lines.Add('ExportDOCX large test analysis');" ^
  "$lines.Add('Generated: ' + (Get-Date));" ^
  "$lines.Add('CSV: ' + $csv);" ^
  "$lines.Add('');" ^
  "$lines.Add('Total CSV rows: ' + $total);" ^
  "$lines.Add('');" ^
  "$lines.Add('[Status counts]');" ^
  "$groups|ForEach-Object { $lines.Add(('{0}: {1}' -f $_.Name,$_.Count)) };" ^
  "$lines.Add('');" ^
  "$lines.Add('[Problem files]');" ^
  "if($bad){$bad|ForEach-Object{$lines.Add(($_.Status + ' | ' + $_.HRESULT + ' | ' + $_.Input + ' | ' + $_.Message + ' | ' + $_.Validation))}} else {$lines.Add('None')};" ^
  "$lines.Add('');" ^
  "$lines.Add('[Files with warnings]');" ^
  "if($warn){$warn|Select-Object -First 200|ForEach-Object{$lines.Add(($_.Input + ' | ' + $_.Validation))}; if($warn.Count -gt 200){$lines.Add('... truncated ...')}} else {$lines.Add('None')};" ^
  "$lines.Add('');" ^
  "$lines.Add('[Slowest files]');" ^
  "$slow|ForEach-Object{$lines.Add(('{0} ms | {1}' -f $_.ElapsedMs,$_.Input))};" ^
  "if(Test-Path -LiteralPath $summary){$lines.Add('');$lines.Add('[Summary.txt]');$lines.AddRange([IO.File]::ReadAllLines($summary))};" ^
  "[IO.File]::WriteAllLines($out,$lines,[Text.UTF8Encoding]::new($true));" ^
  "Write-Host ('Analysis written: ' + $out)"

exit /b %ERRORLEVEL%
