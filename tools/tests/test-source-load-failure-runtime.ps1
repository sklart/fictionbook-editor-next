[CmdletBinding()]
param([string]$FbeExe)
$ErrorActionPreference='Stop'
if (!$FbeExe) { $FbeExe=Join-Path $PSScriptRoot '../../out/Release/FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'source-load-failure'
$passed=$false
try {
  New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
  $valid=Join-Path $iso.Root 'valid.fb2'; $invalid=Join-Path $iso.Root 'invalid.fb2'; $report=Join-Path $iso.Root 'result.txt'
  [IO.File]::WriteAllText($valid,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Source</book-title><lang>en</lang></title-info><document-info><id>probe</id><version>1.0</version></document-info></description><body><section><p>ORIGINAL CONTENT</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText($invalid,'<?xml version="1.0"?><FictionBook><broken>',[Text.UTF8Encoding]::new($false))
  $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='source-load-failure-probe'; $env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH=$invalid
  $p=Start-Process $iso.Exe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$valid) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
  $code=Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'source-load-failure-probe' -Report $report -TimeoutSeconds 150
  $result=Get-Content $report -Raw; Write-Host $result
  if ($code -ne 0) { throw "Source load failure scenario failed: exit=$code" }
  $passed=$true
} finally {
  Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_TEST_SCENARIO,Env:FBE_NEXT_TEST_MALFORMED_SOURCE_PATH -ErrorAction SilentlyContinue
  Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
