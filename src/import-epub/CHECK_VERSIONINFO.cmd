@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "ROOT=%~dp0"
set "DLL=%ROOT%out\Release\ImportEPUB.dll"
set "EXE=%ROOT%out\Release\ImportEPUBBatch.exe"

echo ================================================================
echo ImportEPUB file version resource check
echo ================================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$files = @('%DLL%', '%EXE%');" ^
  "$ok = $true;" ^
  "foreach($f in $files){" ^
  "  Write-Host 'File:' $f;" ^
  "  if(!(Test-Path -LiteralPath $f)){ Write-Host '  MISSING'; $ok = $false; continue }" ^
  "  $v = (Get-Item -LiteralPath $f).VersionInfo;" ^
  "  Write-Host ('  CompanyName:      ' + $v.CompanyName);" ^
  "  Write-Host ('  FileDescription:  ' + $v.FileDescription);" ^
  "  Write-Host ('  FileVersion:      ' + $v.FileVersion);" ^
  "  Write-Host ('  InternalName:     ' + $v.InternalName);" ^
  "  Write-Host ('  OriginalFilename: ' + $v.OriginalFilename);" ^
  "  Write-Host ('  ProductName:      ' + $v.ProductName);" ^
  "  Write-Host ('  ProductVersion:   ' + $v.ProductVersion);" ^
  "  Write-Host '';" ^
  "  if([string]::IsNullOrWhiteSpace($v.CompanyName) -or [string]::IsNullOrWhiteSpace($v.FileDescription)){ $ok = $false }" ^
  "}" ^
  "if(!$ok){ exit 1 }"

if errorlevel 1 (
  echo.
  echo ERROR: version resources are missing or incomplete.
  exit /b 1
)

echo OK: version resources are present.
exit /b 0
