[CmdletBinding()]
param([string]$FbeExe, [switch]$LockedOldSidecar, [switch]$LockedSnapshot,
  [ValidateSet('sidecar-create','sidecar-write','sidecar-flush','sidecar-rename','after-snapshot')][string]$FaultPoint)
$ErrorActionPreference = 'Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $FbeExe) { $FbeExe=Join-Path $repo 'out\Release\FBE.exe' }
. (Join-Path $repo 'tools\tests\RuntimeTestIsolation.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'recovery-mismatch'
$ok=$false
try {
  if ($FaultPoint -and ($LockedOldSidecar -or $LockedSnapshot)) { throw 'Choose one failure point.' }
  if ($FaultPoint) { $env:FBE_NEXT_TEST_RECOVERY_INJECT=$FaultPoint }
  New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
  $bookA=Join-Path $iso.Root 'Archive-A.zip'; $bookB=Join-Path $iso.Root 'Archive-B.zip'
  foreach($pair in @(@($bookA,'RECOVERY_BOOK_A'),@($bookB,'RECOVERY_BOOK_B'))) {
    $stream=[IO.File]::Open($pair[0],[IO.FileMode]::Create)
    try { $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create); try {
      $entry=$zip.CreateEntry('book.fb2'); $writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
      try {$writer.Write(('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Probe</book-title><lang>en</lang></title-info><document-info><id>probe</id><version>1.0</version></document-info></description><body><section><p>{0}</p></section></body></FictionBook>' -f $pair[1]))} finally {$writer.Dispose()}
    } finally {$zip.Dispose()} } finally {$stream.Dispose()}
  }
  if ($LockedOldSidecar) {
    $normal=Join-Path $iso.Root 'Book-B.fb2'
    $archive=[IO.Compression.ZipFile]::OpenRead($bookB)
    try { $entry=$archive.GetEntry('book.fb2'); $reader=[IO.StreamReader]::new($entry.Open()); try { [IO.File]::WriteAllText($normal,$reader.ReadToEnd(),[Text.UTF8Encoding]::new($false)) } finally { $reader.Dispose() } } finally { $archive.Dispose() }
    $second=$normal; $env:FBE_NEXT_TEST_RECOVERY_LOCK_OLD='1'; $normalBefore=(Get-FileHash $normal).Hash
  } else { $second=$bookB; if ($LockedSnapshot) { $env:FBE_NEXT_TEST_RECOVERY_LOCK_SNAPSHOT='1' } }
  $beforeA=(Get-FileHash $bookA).Hash; $beforeB=(Get-FileHash $bookB).Hash
  $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='recovery-mismatch-create'; $env:FBE_NEXT_TEST_RECOVERY_SECOND=$second
  $reportA=Join-Path $iso.Root 'create.txt'
  $p=Start-Process $iso.Exe -ArgumentList ('--portable -b "{0}" "{1}"' -f $reportA,$bookA) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
  $code=Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'recovery-mismatch-create' -Report $reportA -TimeoutSeconds 180
  $create=Get-Content $reportA -Raw; if ($code -ne 0) {throw "create failed $code $create"}
  $snapshot=Join-Path $iso.DataPath 'Recovery\Recovery.fb2'; $sidecar=Join-Path $iso.DataPath 'Recovery\Recovery.archive.txt'
  $snapshotHash=(Get-FileHash $snapshot).Hash; $sidecarHash=if(Test-Path $sidecar){(Get-FileHash $sidecar).Hash}else{'missing'}
  $oldSnapshotHash=(Get-FileHash ($second+'.previous-snapshot')).Hash
  $oldSidecarHash=(Get-FileHash ($second+'.previous-sidecar')).Hash
  if ($LockedOldSidecar) {
    if ($snapshotHash -ne (Get-FileHash ($normal+'.previous-snapshot')).Hash -or $sidecarHash -eq 'missing' -or $sidecarHash -ne (Get-FileHash ($normal+'.previous-sidecar')).Hash) { throw 'Old snapshot and sidecar were not preserved' }
    $env:FBE_NEXT_TEST_SCENARIO='recovery-mismatch-verify-old'
  } elseif ($LockedSnapshot) {
    if ($snapshotHash -ne (Get-FileHash ($bookB+'.previous-snapshot')).Hash -or $sidecarHash -ne 'missing') { throw 'Failed snapshot replacement was not preserved safely' }
    $env:FBE_NEXT_TEST_SCENARIO='recovery-mismatch-verify-old-standalone'
  } else { $env:FBE_NEXT_TEST_SCENARIO='recovery-mismatch-verify' }
  $reportB=Join-Path $iso.Root 'verify.txt'
  $p=Start-Process $iso.Exe -ArgumentList ('--portable -b "{0}"' -f $reportB) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
  $code=Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'recovery-mismatch-verify' -Report $reportB -TimeoutSeconds 180
  $verify=Get-Content $reportB -Raw
  $afterA=(Get-FileHash $bookA).Hash; $afterB=(Get-FileHash $bookB).Hash
  "create=$create verify=$verify exit=$code snapshot_before=$oldSnapshotHash snapshot_after=$snapshotHash sidecar_before=$oldSidecarHash sidecar_after=$sidecarHash A_before=$beforeA A_after=$afterA B_before=$beforeB B_after=$afterB"
  if ($code -ne 0 -or $beforeA -ne $afterA -or $beforeB -ne $afterB -or (!$LockedOldSidecar -and $sidecarHash -ne 'missing') -or ($LockedOldSidecar -and (Get-FileHash $normal).Hash -ne $normalBefore)) {throw 'verify failed or an original changed'}
  $ok=$true
} finally {
  Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_TEST_SCENARIO,Env:FBE_NEXT_TEST_RECOVERY_SECOND,Env:FBE_NEXT_TEST_RECOVERY_LOCK_OLD,Env:FBE_NEXT_TEST_RECOVERY_LOCK_SNAPSHOT,Env:FBE_NEXT_TEST_RECOVERY_INJECT -ErrorAction SilentlyContinue
  Complete-IsolatedFbeRuntime -Isolation $iso -Passed $ok -KeepArtifacts
}
