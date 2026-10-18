[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [ValidateRange(5,45)][int]$TimeoutSeconds=25)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repo ('build/diagnostic-operation-runtime-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $Executable -Name 'diagnostic-operations'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_TEST_SAVE_PATH')
$saved=@{}; foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
function Assert-True($value,[string]$message){if(!$value){throw $message}}
try {
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TRACE='0'
 $fixture=Join-Path $isolation.Root 'operations.fb2'
 [IO.File]::WriteAllText($fixture,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Operations</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE</nickname></author><program-used>FBE</program-used><date value="2026-01-01">1 January 2026</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>operation fixture</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
 foreach($scenario in 'diagnostic-operations-runtime') {
  $env:FBE_NEXT_TEST_SCENARIO=$scenario
  $env:FBE_NEXT_TEST_SAVE_PATH=Join-Path $isolation.Root 'missing-parent/copy.fb2'
  $report=Join-Path $isolation.Root ($scenario+'.txt')
  $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$fixture) -WindowStyle Hidden -PassThru
  if(!$process.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $process.Id -Force;throw "$scenario exceeded $TimeoutSeconds seconds"}
  $process.WaitForExit();$process.Refresh()
  Assert-True ($process.ExitCode -eq 0) "$scenario failed with exit $($process.ExitCode)"
  $rows=[IO.File]::ReadAllText($report)
  Assert-True ($rows -notmatch '(?m)=0\r?$') "$scenario violated an existing editor invariant"
  $metadata=@(Get-ChildItem (Join-Path $isolation.DataPath 'Diagnostics') -Filter 'fbe-session-*.txt' | Where-Object {[IO.File]::ReadAllText($_.FullName) -match "(?m)^PID=$($process.Id)\r?$"})
  Assert-True ($metadata.Count -eq 1) 'Missing unique real editor session'
  $state=[IO.File]::ReadAllText($metadata[0].FullName)
  Assert-True ($state -match 'operation-statistics-source=selected-session-at-shutdown') 'Final operation counters missing'
  Assert-True ($state -match 'active-editor-mode=body') 'Final actual editor mode differs from diagnostic mode'
  Assert-True ($state -match '(?m)^stats-load-completed=[1-9][0-9]*\r?$') 'Real document load was not counted'
  $viewCount=[long]([regex]::Match($state,'(?m)^stats-view-switch-completed=([0-9]+)').Groups[1].Value)
  Assert-True ($viewCount -ge 6) 'Actual view switches were not counted'
  Assert-True ($state -match '(?m)^stats-view-switch-failed=0\r?$') 'Successful transitions were marked failed'
  Assert-True ($state -match '(?m)^stats-validate-completed=[1-9][0-9]*\r?$' -and $state -match '(?m)^stats-validate-failed=0\r?$') 'Successful real validation was not counted'
  Assert-True ($state -match '(?m)^stats-save-failed=[1-9][0-9]*\r?$') 'Failed actual Save As was not counted'
  Copy-Item -LiteralPath $report -Destination $evidence
  Copy-Item -LiteralPath $metadata[0].FullName -Destination (Join-Path $evidence ($scenario+'-session.txt'))
 }
 $passed=$true
 Write-Host "PASS: actual editor modes/transitions, document load, validation and failed Save As; trace disabled. Evidence: $evidence"
} finally {
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
 $resolved=[IO.Path]::GetFullPath($isolation.Root)
 if($resolved -ne [IO.Path]::GetFullPath($isolation.Runtime) -or !([IO.Path]::GetFileName($resolved).StartsWith('fr-')) -or !($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){throw 'Unexpected disposable runtime path'}
 Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
