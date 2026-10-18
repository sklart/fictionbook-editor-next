[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[ValidateRange(5,45)][int]$TimeoutSeconds=30)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $Executable -Name 'diagnostic-startup'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repo ('build/diagnostic-startup-runtime-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$passed=$false
$oldMode,$oldScenario,$oldTrace=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TRACE
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
 $fixture=Join-Path $isolation.Root 'dialog.fb2'; $report=Join-Path $isolation.Root 'dialog.txt'
 [IO.File]::WriteAllText($fixture,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Diagnostics</book-title><lang>en</lang></title-info><document-info><id>diagnostic-startup</id><version>1.0</version></document-info></description><body><section><p>fixture</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
 $crashes=Join-Path $isolation.DataPath 'Diagnostics/Crashes'
 New-Item -ItemType Directory -Path $crashes -Force | Out-Null
 for($i=0;$i -lt 512;$i++) {
  $id=('{0:x8}-0000-0000-0000-000000000001' -f $i)
  [IO.File]::WriteAllText((Join-Path (Split-Path $crashes) "fbe-session-$id.txt"),"session_id=$id`r`nPID=4294967290`r`noutcome=normal`r`n",[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText((Join-Path $crashes ('FBENext-crash-20000101-000000-{0:d3}-pid4294967290.txt' -f $i)),"session_id=$id`r`nException code: 0xC0000005`r`n",[Text.UTF8Encoding]::new($false))
 }
 $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TRACE='0'; $env:FBE_NEXT_TEST_SCENARIO='diagnostic-startup-runtime'
 $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$fixture) -WindowStyle Hidden -PassThru
 $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'diagnostic-startup-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
 if(Test-Path -LiteralPath $report){Copy-Item -LiteralPath $report -Destination $evidence}
 Get-FileHash -LiteralPath $Executable -Algorithm SHA256 | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
 if($exitCode -ne 0){throw "Diagnostic dialog runtime failed: exit $exitCode. Evidence: $evidence"}
 $rows=[IO.File]::ReadAllText($report)
 if($rows -notmatch 'startup_background=1' -or $rows -notmatch 'metadata_reads=0'){throw 'Incomplete startup checks'}
 $passed=$true
 Write-Host "PASS: real editor startup with 512 sessions/crashes; worker discovery, zero metadata reads, title notification. Evidence: $evidence"
} finally {
 $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TRACE=$oldTrace
 $resolved=[IO.Path]::GetFullPath($isolation.Root)
 if(!($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)) -or !([IO.Path]::GetFileName($resolved).StartsWith('fr-'))){throw 'Unexpected disposable runtime path'}
 Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
