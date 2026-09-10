[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$FbeExe)

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
function Invoke-ArchiveFbe([string]$archive, [string]$report, [string]$entry = '') {
    $oldMode,$oldScenario,$oldEntry = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARCHIVE_ENTRY
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='archive-runtime'; if($entry) { $env:FBE_NEXT_TEST_ARCHIVE_ENTRY=$entry } else { Remove-Item Env:FBE_NEXT_TEST_ARCHIVE_ENTRY -ErrorAction SilentlyContinue }
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $archive) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit(90000)) { Stop-Process -Id $process.Id -Force; throw 'FBE archive runtime test timed out.' }
        if($process.ExitCode -ne 0) { throw "FBE archive runtime test exited $($process.ExitCode)." }
    } finally { foreach($pair in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_ARCHIVE_ENTRY',$oldEntry))) { if($null -eq $pair[1]) { Remove-Item ("Env:" + $pair[0]) -ErrorAction SilentlyContinue } else { Set-Item ("Env:" + $pair[0]) $pair[1] } } }
}
try {
    $book = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><body><section><p>ARCHIVE_RUNTIME_BEFORE</p></section></body></FictionBook>'
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
    Write-Host 'FBE.exe archive ZIP runtime integration passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
