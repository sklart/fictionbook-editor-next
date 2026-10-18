[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work=Join-Path $repo ('build/diagnostic-catalog-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_PACKAGE_DIRECTORY','FBE_NEXT_TEST_CATALOG_VERIFY','FBE_NEXT_TRACE','TEMP','TMP')
$saved=@{}; foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
 $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TRACE='0'
 $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $work 'settings'
 $env:FBE_NEXT_TEST_PACKAGE_DIRECTORY=Join-Path $work 'result'
 $env:TEMP=Join-Path $work 'temp'; $env:TMP=$env:TEMP
 New-Item -ItemType Directory -Path $env:TEMP,$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY -Force | Out-Null
 # A fallback Crashes junction must never admit or delete files in its foreign target.
 $foreign=Join-Path $work 'foreign-crashes'; $fallback=Join-Path $env:TEMP 'FBE Next Diagnostics'
 New-Item -ItemType Directory -Path $foreign,$fallback -Force | Out-Null
 $foreignReport=Join-Path $foreign 'FBENext-crash-20000101-000000-999-pid4294967280.txt'
 [IO.File]::WriteAllText($foreignReport,'FOREIGN_PRIVATE_CONTENT',[Text.UTF8Encoding]::new($false))
 $null=New-Item -ItemType Junction -Path (Join-Path $fallback 'Crashes') -Target $foreign
 $hangSession='BBBB0001-0000-0000-0000-000000000001'
 $foreignHang=Join-Path $foreign ('fbe-hang-'+$hangSession+'-1.txt')
 $hangText="hang-format=1`r`nsession_id=$hangSession`r`nhang-state=confirmed`r`nhang-duration-ms=30001`r`nhang-confirmed=1`r`n"
 [IO.File]::WriteAllText($foreignHang,$hangText,[Text.UTF8Encoding]::new($false))
 [IO.File]::SetLastWriteTimeUtc($foreignHang,[datetime]'2000-01-01T00:00:00Z')
 $null=New-Item -ItemType Junction -Path (Join-Path $fallback 'Hangs') -Target $foreign
 foreach($verify in @('0','1')) {
  $env:FBE_NEXT_TEST_CATALOG_VERIFY=$verify
  $p=Start-Process -FilePath $Executable -ArgumentList '--installed','--diagnostic-catalog-test' -WindowStyle Hidden -PassThru
  if(!$p.WaitForExit(30000)){Stop-Process -Id $p.Id -Force;throw "Catalog test timed out: $work"}
  $p.WaitForExit(); if($p.ExitCode -ne 0){throw "Catalog test exit=$($p.ExitCode) verify=$verify. Evidence: $work"}
 }
 if([IO.File]::ReadAllText($foreignReport) -cne 'FOREIGN_PRIVATE_CONTENT'){throw 'Crash junction admitted or changed foreign content'}
 if([IO.File]::ReadAllText($foreignHang) -cne $hangText){throw 'Hang junction admitted or changed foreign content'}
 $catalogEvidence=Get-Content -LiteralPath (Join-Path $env:FBE_NEXT_TEST_PACKAGE_DIRECTORY 'catalog.txt') -Raw
 if($catalogEvidence -notmatch 'lock_release_guards=1'){throw 'Catalog lock release regression did not run'}
 Write-Host $catalogEvidence
 Write-Host "PASS: indexed large catalog; background discovery; damaged review persists without changing bytes; replacement notification; safe TXT/DMP cleanup. Evidence: $work"
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])}}
