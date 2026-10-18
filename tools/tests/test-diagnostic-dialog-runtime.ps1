[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[ValidateRange(5,45)][int]$TimeoutSeconds=30)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $Executable -Name 'diagnostic-dialog'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repo ('build/diagnostic-dialog-runtime-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$passed=$false
$oldBreadcrumb=$env:FBE_NEXT_TEST_STARTUP_BREADCRUMB
$oldMode,$oldScenario,$oldTrace=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TRACE
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
 $fixture=Join-Path $isolation.Root 'dialog.fb2'; $report=Join-Path $isolation.Root 'dialog.txt'
 [IO.File]::WriteAllText($fixture,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Diagnostics</book-title><lang>en</lang></title-info><document-info><id>diagnostic-dialog</id><version>1.0</version></document-info></description><body><section><p>fixture</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
 $crashes=Join-Path $isolation.DataPath 'Diagnostics/Crashes'
 New-Item -ItemType Directory -Path $crashes -Force | Out-Null
 $damaged=Join-Path $crashes 'FBENext-crash-20000101-000000-001-pid4294967290.txt'
 $damagedBytes=[Text.Encoding]::UTF8.GetBytes("session_id=broken`r`nPRIVATE_BOOK C:\secret\book.fb2")
 [IO.File]::WriteAllBytes($damaged,$damagedBytes)
 $validId='DDDD0001-0000-0000-0000-000000000001'
 $valid=Join-Path $crashes 'FBENext-crash-20000101-000000-002-pid4294967291.txt'
 $validBytes=[Text.Encoding]::UTF8.GetBytes("session_id=$validId`r`nException code: 0xC0000005`r`nProcess ID: 4294967291`r`n")
 [IO.File]::WriteAllBytes($valid,$validBytes)
 [IO.File]::WriteAllText((Join-Path (Split-Path $crashes) "fbe-session-$validId.txt"),"session_id=$validId`r`nPID=4294967291`r`nstarted=2000-01-01T00:00:00`r`noutcome=crash`r`n",[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TRACE='0'; $env:FBE_NEXT_TEST_SCENARIO='diagnostic-dialog-runtime'
 $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=Join-Path $evidence 'startup.txt'
 $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$fixture) -WindowStyle Hidden -PassThru
 $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'diagnostic-dialog-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
 if(Test-Path -LiteralPath $report){Copy-Item -LiteralPath $report -Destination $evidence}
 Get-FileHash -LiteralPath $Executable -Algorithm SHA256 | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
 if($exitCode -ne 0){throw "Diagnostic dialog runtime failed: exit $exitCode. Evidence: $evidence"}
 $rows=[IO.File]::ReadAllText($report)
 if([Convert]::ToBase64String([IO.File]::ReadAllBytes($damaged)) -cne [Convert]::ToBase64String($damagedBytes)){throw 'Manual review changed damaged report bytes'}
 if([Convert]::ToBase64String([IO.File]::ReadAllBytes($valid)) -cne [Convert]::ToBase64String($validBytes)){throw 'Automatic review changed report bytes'}
 foreach($check in 'automatic_review','review_worker_thread','review_stale_rejected','review_write_failed') {
  if($rows -notmatch "$check=1"){throw "Incomplete review worker check: $check"}
 }
 if($rows -notmatch 'damaged_review=1'){throw 'Damaged-report review did not preserve snapshot'}
 if($rows -notmatch 'layout_dpi_theme=1' -or $rows -notmatch 'responsive_cancel_close=1'){throw 'Incomplete dialog checks'}
 $passed=$true
 Write-Host "PASS: diagnostic dialog 96/144/192 DPI geometry, 12 locales, light/dark application, worker heartbeat, cancellation and deferred close. Evidence: $evidence"
} finally {
 $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=$oldBreadcrumb
 $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TRACE=$oldTrace
 $resolved=[IO.Path]::GetFullPath($isolation.Root)
 if(!($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)) -or !([IO.Path]::GetFileName($resolved).StartsWith('fr-'))){throw 'Unexpected disposable runtime path'}
 Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
