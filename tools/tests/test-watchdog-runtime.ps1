[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[ValidateRange(10,45)][int]$TimeoutSeconds=30)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$isolation=New-IsolatedFbeRuntime -FbeExe $Executable -Name 'watchdog'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$evidence=Join-Path $repo ('build/watchdog-runtime-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_TEST_WATCHDOG_SHORT','FBE_NEXT_TEST_WATCHDOG_FAULT')
$saved=@{}; foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$passed=$false
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
 $fixture=Join-Path $isolation.Root 'watchdog.fb2'; $report=Join-Path $isolation.Root 'watchdog.txt'
 [IO.File]::WriteAllText($fixture,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>PRIVATE_BOOK</book-title><lang>en</lang></title-info><document-info><id>watchdog</id><version>1.0</version></document-info></description><body><section><p>PRIVATE_BOOK watchdog fixture.</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
 $fixtureHash=(Get-FileHash $fixture -Algorithm SHA256).Hash
 $env:FBE_NEXT_TEST_WATCHDOG_FAULT=$null
 $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TRACE='0'; $env:FBE_NEXT_TEST_SCENARIO='watchdog-runtime'; $env:FBE_NEXT_TEST_WATCHDOG_SHORT='1'
 $process=Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$fixture) -WindowStyle Hidden -PassThru
 $exit=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'watchdog-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
 if(Test-Path -LiteralPath $report){Copy-Item -LiteralPath $report -Destination $evidence}
 $hangs=Join-Path $isolation.DataPath 'Diagnostics/Hangs'
 if(Test-Path -LiteralPath $hangs){Copy-Item -LiteralPath $hangs -Destination $evidence -Recurse}
 if($exit -ne 0){throw "Watchdog runtime failed: exit=$exit. Evidence: $evidence; runtime: $($isolation.Root)"}
 $rows=[IO.File]::ReadAllText($report); Get-Content -LiteralPath $report
 foreach($check in 'lifecycle','directory_failure','restart_after_failure','worker_exception','concurrent_lifecycle','first_heartbeat','transitions','normal','report_while_ui_and_trace_locked','no_duplicates','recovered','reports_complete','pause_recovery','suspend_resume','stop','shutdown_running','document_preserved','privacy','catalog','zip','passed') {
  if($rows -notmatch "(?m)^$check=1\r?$"){throw "Missing watchdog proof: $check"}
 }
 if((Get-FileHash $fixture -Algorithm SHA256).Hash -cne $fixtureHash){throw 'Watchdog changed the input document'}
 if(@(Get-ChildItem -LiteralPath $isolation.DataPath -Recurse -File -Filter '*.dmp').Count){throw 'Watchdog created a dump without consent'}
 $zip=[IO.Compression.ZipFile]::OpenRead($report+'.zip')
 try {
  $hangCount=0; $manifest=$null
  foreach($entry in $zip.Entries) {
   $stream=$entry.Open(); $memory=[IO.MemoryStream]::new()
   try {$stream.CopyTo($memory); $bytes=$memory.ToArray()} finally {$stream.Dispose();$memory.Dispose()}
   $preview=$report+'.'+($entry.FullName -replace '/','_')+'.preview'
   if(!(Test-Path -LiteralPath $preview) -or [Convert]::ToBase64String($bytes) -cne [Convert]::ToBase64String([IO.File]::ReadAllBytes($preview))){throw "Preview differs from ZIP: $($entry.FullName)"}
   $text=[Text.Encoding]::UTF8.GetString($bytes)
   if($text -match 'PRIVATE_BOOK|PRIVATE_USER|C:\\secret|<p>'){throw "Private content in ZIP: $($entry.FullName)"}
   if($entry.FullName -like 'hang/*') {
    $hangCount++
    if([regex]::Matches($text,'(?m)^hang-state=confirmed\r?$').Count -ne 1 -or [regex]::Matches($text,'(?m)^hang-state=recovered\r?$').Count -ne 1){throw 'Continuous hang was duplicated or recovery lost'}
   }
   if($entry.FullName -eq 'manifest.json'){$manifest=$text | ConvertFrom-Json}
  }
  if($hangCount -ne 3 -or $manifest.privacy -ne 'checked-final-bytes' -or @($manifest.omitted | Where-Object {$_.name -like 'hang/*' -and $_.reason -eq 'privacy-rejected'}).Count -ne 1){throw 'Hang ZIP privacy/manifest contract failed'}
 } finally {$zip.Dispose()}
 Copy-Item -LiteralPath ($report+'.zip') -Destination $evidence
 Get-FileHash -LiteralPath $Executable -Algorithm SHA256 | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
 $passed=$true
 Write-Host "PASS: trace-off watchdog, UI + trace-lock stalls, independent reports, recovery, repeat/no duplicates, pause/resume, stop, document preservation, typed privacy and exact preview/ZIP. Evidence: $evidence"
} finally {
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}
 Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
