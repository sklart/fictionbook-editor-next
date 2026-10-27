[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_PACKAGE_DIRECTORY','FBE_NEXT_TEST_PACKAGE_SESSION')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$evidence=Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('build/real-crash-retention-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
try {
 foreach($trace in '0','1') {
  $iso=New-IsolatedFbeRuntime -FbeExe $Executable -Name crash-retention;$passed=$false
  try {
   $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
   $fixture=Join-Path $iso.Root crash.fb2;$report=Join-Path $iso.Root unused.txt
   [IO.File]::WriteAllText($fixture,'<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Controlled crash</book-title><lang>en</lang></title-info><document-info><id>crash</id><version>1.0</version></document-info></description><body><section><p>Synthetic fixture</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
   $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='diagnostic-crash-retention';$env:FBE_NEXT_TRACE=$trace;$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=$null
   $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
   $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 30
   if($exitCode -ne -1073741819){throw "Controlled unhandled exception returned $exitCode"}
   $reports=@(Get-ChildItem -LiteralPath $iso.Root -Recurse -File -Filter 'FBENext-crash-*.txt')
   if($reports.Count -ne 1){throw 'Actual crash report missing or ambiguous'}
   $crash=$reports[0];$text=[IO.File]::ReadAllText($crash.FullName)
   if($text -notmatch 'Exception code: 0xC0000005' -or $text -notmatch 'CP297:.*0x8000FFFF.*stage=CP314.*operation-stage=CP402'){throw 'Actual crash report lost the exception or safe HRESULT metadata'}
   $session=([regex]::Match($text,'(?m)^session_id=([A-Fa-f0-9-]+)')).Groups[1].Value
   if(!$session){throw 'Crash report lost session identity'}
   $dump=[IO.Path]::ChangeExtension($crash.FullName,'.dmp')
   if(!(Test-Path -LiteralPath $dump) -or (Get-Item -LiteralPath $dump).Length -eq 0){throw 'Actual minidump is empty'}
   $hash=(Get-FileHash -LiteralPath $crash.FullName).Hash
   $destination=Join-Path $evidence ('trace-'+$trace);New-Item -ItemType Directory -Path $destination | Out-Null
   Copy-Item -LiteralPath $crash.FullName,$dump -Destination $destination
   $env:FBE_NEXT_TEST_SCENARIO=$null;$env:FBE_NEXT_TRACE='0';$env:FBE_NEXT_TEST_PACKAGE_SESSION=$session;$env:FBE_NEXT_TEST_PACKAGE_DIRECTORY=Join-Path $destination package
   $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable','--diagnostic-package-test') -WindowStyle Hidden -PassThru
   if(!$process.WaitForExit(30000)){Stop-Process -Id $process.Id -Force;throw 'Post-crash package timed out'}
   if($process.ExitCode -ne 0){throw 'Post-crash package failed'}
   $zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $env:FBE_NEXT_TEST_PACKAGE_DIRECTORY package.zip))
   try {
    $entry=$zip.GetEntry('manifest.json');if(!$entry){throw 'Post-crash manifest missing'}
    $reader=[IO.StreamReader]::new($entry.Open());try{$manifest=$reader.ReadToEnd() | ConvertFrom-Json}finally{$reader.Dispose()}
    if($manifest.session_id -ne $session -or $manifest.outcome -ne 'crash'){throw 'Package silently switched to another session'}
    $entry=$zip.GetEntry('crash/latest-crash-report.txt');if(!$entry){throw 'Actual crash report absent from package'}
    $reader=[IO.StreamReader]::new($entry.Open());try{$summary=$reader.ReadToEnd()}finally{$reader.Dispose()}
    if($summary -notmatch '0xC0000005'){throw 'Package lost crash evidence'}
    # The crash projection deliberately omits legacy free text. Typed HRESULT
    # metadata must survive in the typed trace or minimal error-events component instead.
    $events=''
    foreach($logEntry in $zip.Entries | Where-Object {$_.FullName -eq 'plugin-and-error-events.txt' -or $_.FullName -like 'trace/*.log'}) {
     $reader=[IO.StreamReader]::new($logEntry.Open());try{$events+=$reader.ReadToEnd()}finally{$reader.Dispose()}
    }
    if($events -notmatch 'code=CP297;.*hr=0x8000FFFF;.*stage=CP314;.*operation-stage=CP402'){throw 'Package lost safe HRESULT/stage/route evidence'}
    if($trace -eq '1' -and @($zip.Entries | Where-Object {$_.FullName -like 'trace/*.log'}).Count -eq 0){throw 'Enabled selected session trace missing'}
    if($trace -eq '0' -and !$zip.GetEntry('trace/absent.txt')){throw 'Trace-off package must explicitly mark absent trace while keeping crash data'}
   } finally {$zip.Dispose()}
   if((Get-FileHash -LiteralPath $crash.FullName).Hash -ne $hash){throw 'Package creation changed the original crash report'}
   $passed=$true;Write-Host "PASS: actual unhandled exception, nonempty dump, safe HRESULT/stage/route, persisted session and selected package; trace=$trace. $destination"
  } finally {Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
 }
 Get-FileHash -LiteralPath $Executable | Format-List | Out-File (Join-Path $evidence executable-sha256.txt)
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}}
