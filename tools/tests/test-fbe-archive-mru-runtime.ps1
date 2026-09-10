[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$FbeExe, [ValidateRange(150, 300)][int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-archive-mru-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function New-Zip([string]$path, [object[]]$entries) {
    $stream = [IO.File]::Open($path, [IO.FileMode]::Create)
    try { $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create); try { foreach($pair in $entries) { $writer = [IO.StreamWriter]::new(($zip.CreateEntry([string]$pair[0])).Open(), [Text.UTF8Encoding]::new($false)); try { $writer.Write([string]$pair[1]) } finally { $writer.Dispose() } } } finally { $zip.Dispose() } } finally { $stream.Dispose() }
}
function Quote([string]$value) { '"' + $value.Replace('"', '\"') + '"' }
function Invoke-Mru([string]$archive, [string]$first, [int]$firstOccurrence, [string]$second, [int]$secondOccurrence, [string]$report) {
    $saved = @{}; foreach($name in 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_ARCHIVE_ENTRY','FBE_NEXT_TEST_ARCHIVE_OCCURRENCE','FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY','FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE') { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'archive-mru-runtime'; $env:FBE_NEXT_TEST_ARCHIVE_ENTRY = $first; $env:FBE_NEXT_TEST_ARCHIVE_OCCURRENCE = "$firstOccurrence"; $env:FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY = $second; $env:FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE = "$secondOccurrence"
        $process = Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b {0} {1}' -f (Quote $report), (Quote $archive)) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE archive MRU runtime test timed out.' }
        $state = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if ($process.ExitCode -ne 0 -or $state -notmatch 'first=1' -or $state -notmatch 'second=1' -or $state -notmatch 'reopened_first=1' -or $state -notmatch 'missing_entry=1') { throw "Archive MRU runtime failure: $state" }
    } finally { foreach($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') } }
}
try {
    $portableIni = Join-Path (Split-Path $FbeExe) 'portable.ini'; $hadPortableIni = Test-Path -LiteralPath $portableIni; $oldPortableIni = if($hadPortableIni) { Get-Content -LiteralPath $portableIni -Raw } else { $null }; $data = 'ArchiveMruRuntimeData'
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=$data`r`n", [Text.UTF8Encoding]::new($false)); New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path $FbeExe) $data) | Out-Null
    $book = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Mru</nickname></author><book-title>MRU</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE</nickname></author><program-used>FBE</program-used><date value="2026-09-10">10</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>MRU</p></section></body></FictionBook>'
    $unicodeZip = Join-Path $root 'книги.zip'; New-Zip $unicodeZip ([object[]]@([object[]]@('folder/Книга.fb2', $book), [object[]]@('second.fb2', $book))); $check = [IO.Compression.ZipFile]::OpenRead($unicodeZip); try { if(@($check.Entries.FullName) -notcontains 'second.fb2') { throw 'MRU ZIP fixture did not contain second.fb2.' } } finally { $check.Dispose() }; Invoke-Mru $unicodeZip 'folder/Книга.fb2' 0 'second.fb2' 1 (Join-Path $root 'unicode.txt')
    $duplicates = Join-Path $root 'duplicates.zip'; New-Zip $duplicates ([object[]]@([object[]]@('duplicate.fb2', $book), [object[]]@('duplicate.fb2', $book))); Invoke-Mru $duplicates 'duplicate.fb2' 0 'duplicate.fb2' 1 (Join-Path $root 'duplicate.txt')
    $mru = Get-ChildItem -LiteralPath (Join-Path (Split-Path $FbeExe) $data) -Recurse -Filter ArchiveMRU.txt | Select-Object -First 1
    if($null -eq $mru) { throw 'Versioned archive MRU file was not persisted.' }
    $persisted = Get-Content -LiteralPath $mru.FullName -Raw -Encoding Unicode
    if($persisted -notmatch 'FBE-ARCHIVE-MRU\t2' -or ([regex]::Matches($persisted, 'duplicate\.fb2')).Count -ne 2 -or $persisted -notmatch 'folder/Книга\.fb2') { throw 'Archive MRU persistence lost independent or Unicode identities.' }
    Write-Host 'FBE.exe archive MRU runtime regression passed.'
} finally { if($hadPortableIni) { [IO.File]::WriteAllText($portableIni, $oldPortableIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }; Remove-Item -LiteralPath (Join-Path (Split-Path $FbeExe) 'ArchiveMruRuntimeData') -Recurse -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
