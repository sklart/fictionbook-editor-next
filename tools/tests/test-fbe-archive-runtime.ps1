[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$FbeExe,
    [ValidateRange(150, 300)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path ([IO.Path]::GetTempPath()) ("fbe-archive-runtime-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function New-Zip([string]$path, [hashtable]$entries) {
    $stream = [IO.File]::Open($path, [IO.FileMode]::Create)
    try { $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create); try { foreach($name in $entries.Keys) { $entry = $zip.CreateEntry($name); $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false)); try { $writer.Write([string]$entries[$name]) } finally { $writer.Dispose() } } } finally { $zip.Dispose() } } finally { $stream.Dispose() }
}
function Read-Zip([string]$path) {
    $result = @{}; $zip = [IO.Compression.ZipFile]::OpenRead($path); try { foreach($entry in $zip.Entries) { $reader = [IO.StreamReader]::new($entry.Open(), [Text.UTF8Encoding]::new($false)); try { $result[$entry.FullName] = $reader.ReadToEnd() } finally { $reader.Dispose() } } } finally { $zip.Dispose() }; return $result
}
function Expand-ArchiveFixture([string]$fixture, [string]$destination) {
    [IO.File]::WriteAllBytes($destination, [Convert]::FromBase64String((Get-Content -LiteralPath $fixture -Raw).Trim()))
}
function ConvertTo-FbeCommandLineArgument([string]$value) {
    # Start-Process joins an ArgumentList array before CreateProcess.  Quote
    # paths explicitly so archive names with spaces and Unicode remain one CLI
    # argument (the production shell already does this).
    return '"' + $value.Replace('"', '\"') + '"'
}
function Invoke-ArchiveFbe([string]$archive, [string]$report, [string]$entry = '', [string]$scenario = 'archive-runtime', [string]$savePath = '') {
    $oldMode,$oldScenario,$oldEntry,$oldSavePath = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARCHIVE_ENTRY,$env:FBE_NEXT_TEST_SAVE_PATH
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=$scenario; if($entry) { $env:FBE_NEXT_TEST_ARCHIVE_ENTRY=$entry } else { Remove-Item Env:FBE_NEXT_TEST_ARCHIVE_ENTRY -ErrorAction SilentlyContinue }; if($savePath) { $env:FBE_NEXT_TEST_SAVE_PATH=$savePath } else { Remove-Item Env:FBE_NEXT_TEST_SAVE_PATH -ErrorAction SilentlyContinue }
        # Portable state isolates MRU/recovery from the developer profile.  The
        # watchdog must exceed FBE's own 120-second first-MSHTML timeout.
        $arguments = '--portable -b {0} {1}' -f (ConvertTo-FbeCommandLineArgument $report), (ConvertTo-FbeCommandLineArgument $archive)
        $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE archive runtime test timed out after $TimeoutSeconds seconds." }
        if($process.ExitCode -ne 0) { $details = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report was not created' }; throw "FBE archive runtime test exited $($process.ExitCode): $details" }
    } finally { foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_ARCHIVE_ENTRY',$oldEntry),@('FBE_NEXT_TEST_SAVE_PATH',$oldSavePath))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } } }
}
function Invoke-TwoPhaseFbe([string]$document, [string]$failedArchive, [string]$report) {
    $oldMode,$oldScenario,$oldFailure = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='archive-two-phase-runtime'; $env:FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH=$failedArchive
        $arguments = '--portable -b {0} {1}' -f (ConvertTo-FbeCommandLineArgument $report), (ConvertTo-FbeCommandLineArgument $document)
        $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE two-phase runtime test timed out after $TimeoutSeconds seconds." }
        if($process.ExitCode -ne 0) { $details = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report was not created' }; throw "FBE two-phase runtime test exited $($process.ExitCode): $details" }
    } finally { foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH',$oldFailure))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } } }
}
function Invoke-RecoveryFbe([string]$scenario, [string]$report, [string]$archive = '') {
    $oldMode,$oldScenario = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=$scenario
        $arguments = '--portable -b {0}' -f (ConvertTo-FbeCommandLineArgument $report)
        if($archive) { $arguments += ' ' + (ConvertTo-FbeCommandLineArgument $archive) }
        $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE recovery runtime test timed out after $TimeoutSeconds seconds." }
        if($process.ExitCode -ne 0) { $details = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { 'report was not created' }; throw "FBE recovery runtime test exited $($process.ExitCode): $details" }
    } finally { foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } } }
}
try {
    $portableIni = Join-Path (Split-Path $FbeExe) 'portable.ini'; $hadPortableIni = Test-Path -LiteralPath $portableIni; $oldPortableIni = if($hadPortableIni) { Get-Content -LiteralPath $portableIni -Raw } else { $null }
    $portableData = 'ArchiveRuntimeData'; [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=$portableData`r`n", [Text.UTF8Encoding]::new($false)); New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path $FbeExe) $portableData) | Out-Null
    $book = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Archive runtime</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE Next</nickname></author><program-used>FBE Next</program-used><date value="2026-09-10">10 September 2026</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>ARCHIVE_RUNTIME_BEFORE</p></section></body></FictionBook>'
    $zip = Join-Path $root 'single.zip'; $report = Join-Path $root 'single.txt'; New-Zip $zip @{ 'book.fb2'=$book; 'cover.txt'='unchanged' }; $before = Read-Zip $zip
    Invoke-ArchiveFbe $zip $report
    $state = Get-Content -LiteralPath $report -Raw
    foreach($line in @('archive=1','fb2=1','mshtml=1','entry=book.fb2','saved=1')) { if($state -notmatch [regex]::Escape($line)) { throw "ZIP open/save runtime report missing $line" } }
    $after = Read-Zip $zip
    if($after['book.fb2'] -notmatch 'ARCHIVE_RUNTIME_AFTER' -or $after['cover.txt'] -cne $before['cover.txt']) { throw 'ZIP Ctrl+S did not isolate the selected entry.' }
    $multi = Join-Path $root 'multi.zip'; $multiReport = Join-Path $root 'multi.txt'; New-Zip $multi @{ 'first.fb2'=$book; 'selected.fb2'=$book }
    Invoke-ArchiveFbe $multi $multiReport 'selected.fb2'
    $multiState = Get-Content -LiteralPath $multiReport -Raw; if($multiState -notmatch 'entry=selected.fb2') { throw 'Test-mode did not select the requested archive entry.' }
    $multiAfter = Read-Zip $multi; if($multiAfter['selected.fb2'] -notmatch 'ARCHIVE_RUNTIME_AFTER' -or $multiAfter['first.fb2'] -notmatch 'ARCHIVE_RUNTIME_BEFORE') { throw 'Multi-entry ZIP rewrite selected the wrong entry.' }
    $ordinary = Join-Path $root 'ordinary.fb2'; [IO.File]::WriteAllText($ordinary, $book, [Text.UTF8Encoding]::new($false)); $empty = Join-Path $root 'empty.zip'; New-Zip $empty @{ 'readme.txt'='no FictionBook here' }
    $twoPhaseReport = Join-Path $root 'two-phase.txt'; Invoke-TwoPhaseFbe $ordinary $empty $twoPhaseReport
    $twoPhase = Get-Content -LiteralPath $twoPhaseReport -Raw
    foreach($line in @('open_cancelled=1','modified=1','same_document=1','mru_unchanged=1')) { if($twoPhase -notmatch [regex]::Escape($line)) { throw "Two-phase archive open regression: $line" } }
    $rar5 = Join-Path $root 'fixture-rar5.rar'; Expand-ArchiveFixture (Join-Path $PSScriptRoot 'fixtures\archive-runtime-rar5.b64') $rar5
    $rarReport = Join-Path $root 'rar5.txt'; Invoke-ArchiveFbe $rar5 $rarReport 'book.fb2' 'archive-open-runtime'; $rarState = Get-Content -LiteralPath $rarReport -Raw
    foreach($line in @('archive=1','fb2=1','mshtml=1','rar=1','entry=book.fb2')) { if($rarState -notmatch [regex]::Escape($line)) { throw "RAR5 runtime open regression: $line" } }
    $rarBefore = (Get-FileHash -LiteralPath $rar5 -Algorithm SHA256).Hash; $rarOutput = Join-Path $root 'rar-save-as.fb2'; $rarSaveReport = Join-Path $root 'rar-save-as.txt'
    Invoke-ArchiveFbe $rar5 $rarSaveReport 'book.fb2' 'archive-rar-save-runtime' $rarOutput
    $rarSaveState = Get-Content -LiteralPath $rarSaveReport -Raw
    foreach($line in @('archive=1','fb2=1','rar=1','save_as=1','saved=1')) { if($rarSaveState -notmatch [regex]::Escape($line)) { throw "RAR Save As runtime regression: $line`n$rarSaveState" } }
    if(-not (Test-Path -LiteralPath $rarOutput) -or (Get-Content -LiteralPath $rarOutput -Raw) -notmatch 'ARCHIVE_RUNTIME_AFTER') { throw 'RAR Save As did not create the edited FB2.' }
    if((Get-FileHash -LiteralPath $rar5 -Algorithm SHA256).Hash -cne $rarBefore) { throw 'RAR Save As modified the source archive.' }
    $rar5Multi = Join-Path $root 'fixture-rar5-multi.rar'; Expand-ArchiveFixture (Join-Path $PSScriptRoot 'fixtures\archive-runtime-rar5-multi.b64') $rar5Multi
    $rar5MultiReport = Join-Path $root 'rar5-multi.txt'; Invoke-ArchiveFbe $rar5Multi $rar5MultiReport 'book.fbd' 'archive-open-runtime'; $rar5MultiState = Get-Content -LiteralPath $rar5MultiReport -Raw
    foreach($line in @('archive=1','fb2=0','fbd=1','mshtml=1','rar=1','entry=book.fbd')) { if($rar5MultiState -notmatch [regex]::Escape($line)) { throw "RAR5 FBD multi-entry runtime regression: $line" } }
    $recovery = Join-Path $root 'recovery.zip'; New-Zip $recovery @{ 'book.fb2'=$book }; $created = Join-Path $root 'recovery-created.txt'; Invoke-RecoveryFbe 'archive-recovery-create' $created $recovery
    if((Get-Content -LiteralPath $created -Raw) -notmatch 'recovery_created=1') { throw 'Archive recovery was not created.' }
    $restored = Join-Path $root 'recovery-restored.txt'; Invoke-RecoveryFbe 'archive-recovery-verify' $restored
    foreach($line in @('archive=1','fbd=0','recovery_payload=1')) { if((Get-Content -LiteralPath $restored -Raw) -notmatch [regex]::Escape($line)) { throw "Archive recovery FB2 regression: $line" } }
    $fbd = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'fixtures\fbd\description_only.fbd') -Raw -Encoding UTF8; $recoveryFbd = Join-Path $root 'recovery-fbd.zip'; New-Zip $recoveryFbd @{ 'book.fbd'=$fbd }
    $fbdCreated = Join-Path $root 'recovery-fbd-created.txt'; Invoke-RecoveryFbe 'archive-recovery-create' $fbdCreated $recoveryFbd
    $fbdRestored = Join-Path $root 'recovery-fbd-restored.txt'; Invoke-RecoveryFbe 'archive-recovery-verify' $fbdRestored
    foreach($line in @('archive=1','fbd=1','recovery_payload=1')) { if((Get-Content -LiteralPath $fbdRestored -Raw) -notmatch [regex]::Escape($line)) { throw "Archive recovery FBD regression: $line" } }
    $external = Join-Path $root 'recovery-external.zip'; New-Zip $external @{ 'book.fb2'=$book }; $externalCreated = Join-Path $root 'recovery-external-created.txt'; Invoke-RecoveryFbe 'archive-recovery-create' $externalCreated $external
    New-Zip $external @{ 'book.fb2'=($book + '<!-- externally changed -->'); 'cover.txt'='external' }
    $externalSnapshot = [IO.File]::ReadAllBytes($external); $externalHash = (Get-FileHash -LiteralPath $external -Algorithm SHA256).Hash
    $externalReport = Join-Path $root 'recovery-external-restored.txt'; Invoke-RecoveryFbe 'archive-recovery-external-verify' $externalReport
    foreach($line in @('archive=1','blocked=1','modified_externally=1')) { if((Get-Content -LiteralPath $externalReport -Raw) -notmatch [regex]::Escape($line)) { throw "Archive recovery external-modification regression: $line" } }
    if(-not [Linq.Enumerable]::SequenceEqual([byte[]]$externalSnapshot, [IO.File]::ReadAllBytes($external)) -or (Get-FileHash -LiteralPath $external -Algorithm SHA256).Hash -cne $externalHash) { throw 'Blocked recovery Save changed the externally modified ZIP.' }
    Write-Host 'FBE.exe archive ZIP and two-phase runtime integration passed.'
} finally { if($hadPortableIni) { [IO.File]::WriteAllText($portableIni, $oldPortableIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }; Remove-Item -LiteralPath (Join-Path (Split-Path $FbeExe) $portableData) -Recurse -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
