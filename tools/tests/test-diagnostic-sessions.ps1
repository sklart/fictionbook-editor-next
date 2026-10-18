[CmdletBinding()]
param([string]$Executable = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [ValidateSet('All','Basic','Active')][string]$Scenario='All')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work=Join-Path $root ('build/diagnostic-session-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_PACKAGE_DIRECTORY','FBE_NEXT_TEST_PACKAGE_SESSION','FBE_NEXT_TRACE','FBE_NEXT_TEST_PACKAGE_CLEANUP','TEMP','TMP')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
function Assert-True($value,[string]$message){if(!$value){throw $message}}
function Run-Export([string]$name,[bool]$trace,[string]$selected='', [int]$expected=0) {
 $output=Join-Path $work $name
 $env:FBE_NEXT_TEST_PACKAGE_DIRECTORY=$output
 $env:FBE_NEXT_TEST_PACKAGE_SESSION=$selected
 $env:FBE_NEXT_TRACE=if($trace){'1'}else{'0'}
 $p=Start-Process -FilePath $Executable -ArgumentList '--installed','--diagnostic-package-test' -WindowStyle Hidden -PassThru
 if(!$p.WaitForExit(20000)){Stop-Process -Id $p.Id -Force;throw 'Diagnostic test exceeded 20 seconds'}
 Assert-True ($p.ExitCode -eq $expected) "$name returned $($p.ExitCode), expected $expected. $output"
 if($expected -ne 0){return $null}
 $zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $output 'package.zip'))
 try {
  $contents=@{}
  foreach($entry in $zip.Entries){
   $stream=$entry.Open();$memory=[IO.MemoryStream]::new();try{$stream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
   $preview=Join-Path $output (($entry.FullName -replace '/','_')+'.preview')
   Assert-True (Test-Path -LiteralPath $preview) "Unlisted ZIP entry $($entry.FullName)"
   Assert-True ([Convert]::ToBase64String($bytes) -ceq [Convert]::ToBase64String([IO.File]::ReadAllBytes($preview))) "Preview differs: $($entry.FullName)"
   $text=[Text.Encoding]::UTF8.GetString($bytes)
   Assert-True ($text -notmatch 'PRIVATE_USER|PRIVATE_BOOK|C:\\secret') "Unsafe data or post-preview event: $($entry.FullName)"
   Assert-True ($entry.FullName -notmatch '\.dmp$') 'Dump in standard package'
   if(!$selected){Assert-True ($text -notmatch 'DGAFTER') "Post-preview event entered current snapshot: $($entry.FullName)"}
   $contents[$entry.FullName]=$text
  }
  $manifest=$contents['manifest.json'] | ConvertFrom-Json
  Assert-True ($manifest.session_id -match '^[0-9a-fA-F-]{36}$') 'Missing session ID'
  Assert-True ($manifest.privacy -eq 'checked-final-bytes') 'Missing privacy result'
  $stats=$contents['operation-statistics.txt']
  Assert-True ($stats -match "(?m)^session_id=$($manifest.session_id)\r?$") 'Statistics belong to another session'
  if(!$selected) {
   Assert-True ($stats -match 'operation-statistics-source=selected-current-session-at-preview') 'Missing current statistics source'
   Assert-True ($stats -match 'active-editor-mode=source') 'Active source mode absent from preview'
   Assert-True ($contents['runtime-state.txt'] -match 'active-editor-mode=source') 'Runtime mode differs from statistics snapshot'
   Assert-True ($stats -match 'stats-save-completed=2' -and $stats -match 'stats-save-failed=1') 'Operation success/failure counters missing'
   Assert-True ($stats -match 'stats-validate-completed=32' -and $stats -match 'stats-dropped=1') 'Bounded operation tracker failed'
   Assert-True ($stats -match 'stats-view-switch-started=0') 'Operation after preview leaked into ZIP'
  } elseif($stats -match 'operation-statistics-source=selected-session-at-shutdown') {
   Assert-True ($stats -match 'active-editor-mode=body' -and $stats -match 'stats-view-switch-completed=1') 'Historical statistics lost shutdown state'
  }
  foreach($file in $manifest.files){Assert-True ($zip.GetEntry($file.name).Length -eq $file.size) "Manifest size mismatch: $($file.name)"}
  return @{Id=$manifest.session_id;Contents=$contents;Manifest=$manifest}
 }finally{$zip.Dispose()}
}
try {
 $env:TEMP=Join-Path $work 'temp'; $env:TMP=$env:TEMP
 New-Item -ItemType Directory -Path $env:TEMP -Force | Out-Null
 $env:FBE_NEXT_TEST_PACKAGE_CLEANUP='0'
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $work 'settings'
 New-Item -ItemType Directory -Path $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY -Force | Out-Null
 $base=Run-Export 'basic' $false
 Assert-True ($base.Contents.ContainsKey('trace/absent.txt')) 'Basic package must explicitly report missing trace'
 Assert-True ($base.Contents['plugin-and-error-events.txt'] -match 'FD203.*hr=0x80070005') 'Host did not collect plug-in error'
 if($Scenario -eq 'Basic'){Write-Host 'PASS: basic package without extended trace';return}
 $active=Run-Export 'active' $true
 Assert-True ($base.Id -ne $active.Id) 'Session ID reused'
 Assert-True (@($active.Contents.Keys | Where-Object {$_ -like 'trace/*.log'}).Count -eq 1) 'Selected trace missing'
 if($Scenario -eq 'Active'){Write-Host 'PASS: active session and preview/ZIP identity';return}
 $diagnostics=Join-Path $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY 'Diagnostics'
 $activeLog=Get-ChildItem $diagnostics -Filter 'fbe-trace-*.log' | Select-Object -First 1
 $text=[IO.File]::ReadAllText($activeLog.FullName)
 $stem=[IO.Path]::Combine($diagnostics,$activeLog.BaseName)
 foreach($part in 2,10){[IO.File]::WriteAllText("$stem-part$part.log",$text,[Text.UTF8Encoding]::new($false))}
 [IO.File]::WriteAllText("$stem-1.log",'neighbor session',[Text.UTF8Encoding]::new($false))
 [IO.File]::WriteAllText("$stem-part3.log",($text.Replace($active.Id,$base.Id)),[Text.UTF8Encoding]::new($false))
 [IO.File]::WriteAllText("$stem-part4.log",($text+"arbitrary=unstructured private book text`r`n"),[Text.UTF8Encoding]::new($false))
 $previous=Run-Export 'previous' $false $active.Id
 $parts=@($previous.Manifest.files.name | Where-Object {$_ -like 'trace/*.log'})
 Assert-True ($parts.Count -eq 3 -and $parts[1] -like '*-part2.log' -and $parts[2] -like '*-part10.log') 'Wrong segment ordering or neighboring session included'
 Assert-True (@($previous.Manifest.omitted | Where-Object reason -eq 'session-mismatch').Count -eq 1) 'Foreign segment was not explicitly rejected'
 Assert-True (@($previous.Manifest.omitted | Where-Object reason -eq 'privacy-rejected').Count -eq 1) 'Plain unstructured text was not explicitly rejected'

 Assert-True ($previous.Id -eq $active.Id) 'Selected session replaced silently'
 Assert-True ($previous.Manifest.outcome -eq 'normal') 'Normal shutdown was not recorded'
 $diagnostics=Join-Path $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY 'Diagnostics'
 $metadata=Join-Path $diagnostics ('fbe-session-'+$active.Id+'.txt')
 $pidValue=([regex]::Match([IO.File]::ReadAllText($metadata),'(?m)^PID=(\d+)')).Groups[1].Value
 $crashes=Join-Path $diagnostics 'Crashes';New-Item -ItemType Directory -Path $crashes -Force | Out-Null
 # Different explicit ID wins over a reused PID, regardless of matching timestamps.
 $wrong=Join-Path $crashes ("FBENext-crash-20000101-000000-000-pid$pidValue.txt")
 [IO.File]::WriteAllText($wrong,"session_id=$($base.Id)`r`nException code: 0xBAD00001`r`n",[Text.Encoding]::Unicode)
 $unmatched=Run-Export 'wrong-crash' $false $active.Id
 Assert-True (!$unmatched.Contents.ContainsKey('crash/latest-crash-report.txt')) 'Crash matched by PID despite explicit foreign ID'
 $right=Join-Path $crashes ("FBENext-crash-20000101-000001-000-pid99999.txt")
 [IO.File]::WriteAllText($right,"session_id=$($active.Id)`r`nException code: 0xC0000005`r`nProcess ID: $pidValue`r`nPRIVATE_USER <p>PRIVATE_BOOK</p>`r`n",[Text.Encoding]::Unicode)
 $matched=Run-Export 'right-crash' $false $active.Id
 Assert-True ($matched.Contents['crash/latest-crash-report.txt'] -match '0xC0000005') 'Crash did not match by session ID'
 Assert-True ($matched.Manifest.outcome -eq 'crash') 'Crash outcome missing'
 $duplicate=Join-Path $crashes 'FBENext-crash-20000101-000002-000-pid99999.txt'
 Copy-Item -LiteralPath $right -Destination $duplicate
 $ambiguous=Run-Export 'ambiguous-crash' $false $active.Id
 Assert-True (!$ambiguous.Contents.ContainsKey('crash/latest-crash-report.txt')) 'Ambiguous crash was silently selected'
 Assert-True (@($ambiguous.Manifest.omitted | Where-Object reason -eq 'ambiguous-crash-match').Count -eq 1) 'Ambiguous crash reason missing'
 Remove-Item -LiteralPath $duplicate
 $metadataBytes=[IO.File]::ReadAllBytes($metadata)
 Remove-Item -LiteralPath $metadata
 $missing=Run-Export 'missing-metadata' $false $active.Id
 Assert-True ($missing.Contents['environment-report.txt'] -match 'environment-source=historical-session-unavailable') 'Historical environment replaced with current data'
 Assert-True ($missing.Contents['environment-report.txt'] -match 'metadata-status=missing') 'Missing historical metadata not reported'
 [IO.File]::WriteAllBytes($metadata,$metadataBytes)
 # Plain book text without suspicious XML/path tokens must also fail the schema.
 [IO.File]::AppendAllText($metadata,"stats-save-last-ms=PRIVATE_BOOK`r`n",[Text.Encoding]::UTF8)
 $null=Run-Export 'unsafe-statistics' $false $active.Id 94
 [IO.File]::WriteAllBytes($metadata,$metadataBytes)
 [IO.File]::AppendAllText($metadata,"arbitrary=PRIVATE_BOOK`r`n",[Text.Encoding]::UTF8)
 $null=Run-Export 'unsafe-metadata' $false $active.Id 94
 # Manual cleanup may remove only recognized old diagnostics, preserving crash sessions and current data.
 $sentinel=Join-Path $diagnostics 'user-book.fb2';[IO.File]::WriteAllText($sentinel,'preserve')
 $env:FBE_NEXT_TEST_PACKAGE_CLEANUP='1'
 $cleanup=Run-Export 'cleanup' $false
 $cleanupReport=Get-Content -Raw (Join-Path $work 'cleanup/result.txt')
 Assert-True ($cleanupReport -match 'cleanup-files=[1-9]') 'Cleanup did not exercise any deletion'
 Assert-True (Test-Path -LiteralPath $metadata) 'Cleanup deleted a crash session'
 Assert-True (Test-Path -LiteralPath $right) 'Cleanup deleted crash evidence'
 Assert-True (Test-Path -LiteralPath $sentinel) 'Cleanup crossed the diagnostic allowlist'
 Assert-True (Test-Path -LiteralPath (Join-Path $diagnostics ('fbe-session-'+$cleanup.Id+'.txt'))) 'Cleanup deleted current session'
 Write-Host "PASS: basic, selected session, crash identity, typed privacy, plug-in routing, immutable preview/ZIP, manifest sizes. Evidence: $work"
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}}
