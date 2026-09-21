[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$FbeExe, [ValidateRange(150, 300)][int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-archive-mru-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function New-Zip([string]$path, [object[]]$entries) {
    $stream = [IO.File]::Open($path, [IO.FileMode]::Create)
    try { $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create); try { foreach($pair in $entries) { $writer = [IO.StreamWriter]::new(($zip.CreateEntry([string]$pair[0])).Open(), [Text.UTF8Encoding]::new($false)); try { $writer.Write([string]$pair[1]) } finally { $writer.Dispose() } } } finally { $zip.Dispose() } } finally { $stream.Dispose() }
}
function Quote([string]$value) { '"' + $value.Replace('"', '\"') + '"' }
function Read-ReportMap([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "MRU report is missing: $Path" }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) { if ($line -match '^([^=]+)=(.*)$') { $values[$matches[1]] = $matches[2] } }
    return $values
}
function Assert-MruRestart([int]$ExitCode, [string]$Report) {
    $values = Read-ReportMap $Report
    $required = @('count','menu_count','order','clean','folders','reopened','localized_nonempty','russian_empty','english_empty')
    $failed = @()
    foreach ($field in $required) { if ($values[$field] -ne '1' -and ($field -eq 'count' -or $field -eq 'menu_count')) { if ($values[$field] -ne '10') { $failed += $field } } elseif ($field -notin @('count','menu_count') -and $values[$field] -ne '1') { $failed += $field } }
    if ($ExitCode -ne 0 -or $failed.Count -gt 0) {
        $details = @("process_exit = $ExitCode") + ($required | ForEach-Object { "$_ = $($values[$_])" })
        throw "MRU restart failed ($($failed -join ', ')):`n  $($details -join "`n  ")"
    }
}
function Invoke-Mru([string]$archive, [string]$first, [int]$firstOccurrence, [string]$second, [int]$secondOccurrence, [string]$report, [string]$normalEntries = '') {
	$saved = @{}; foreach($name in 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_ARCHIVE_ENTRY','FBE_NEXT_TEST_ARCHIVE_OCCURRENCE','FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY','FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE','FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES') { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    try {
		$env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'archive-mru-runtime'; $env:FBE_NEXT_TEST_ARCHIVE_ENTRY = $first; $env:FBE_NEXT_TEST_ARCHIVE_OCCURRENCE = "$firstOccurrence"; $env:FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY = $second; $env:FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE = "$secondOccurrence"; $env:FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES = $normalEntries
        $process = Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b {0} {1}' -f (Quote $report), (Quote $archive)) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'archive-mru-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        $state = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
		if ($exitCode -ne 0 -or $state -notmatch 'first=1' -or $state -notmatch 'second=1' -or $state -notmatch 'reopened_first=1' -or $state -notmatch 'missing_entry=1' -or $state -notmatch 'menu_clean=1' -or $state -notmatch 'captions_distinct=1') { throw "Archive MRU runtime failure: process_exit=$exitCode`n$state" }
    } finally { foreach($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') } }
}
$isolation = $null
$passed = $false
try {
    $isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'ArchiveMruRuntimeData'; $FbeExe = $isolation.Exe; $data = $isolation.DataPath
    $book = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Mru</nickname></author><book-title>MRU</book-title><lang>en</lang></title-info><document-info><author><nickname>FBE</nickname></author><program-used>FBE</program-used><date value="2026-09-10">10</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>MRU</p></section></body></FictionBook>'
	$normalPaths = @(); for($index = 1; $index -le 9; ++$index) { $normal = Join-Path $root ("обычный-{0}.fb2" -f $index); [IO.File]::WriteAllText($normal, $book, [Text.UTF8Encoding]::new($false)); $normalPaths += $normal }
	$unicodeZip = Join-Path $root 'книги.zip'; New-Zip $unicodeZip ([object[]]@([object[]]@('folder/Книга.fb2', $book), [object[]]@('second.fb2', $book))); $check = [IO.Compression.ZipFile]::OpenRead($unicodeZip); try { if(@($check.Entries.FullName) -notcontains 'second.fb2') { throw 'MRU ZIP fixture did not contain second.fb2.' } } finally { $check.Dispose() }; Invoke-Mru $unicodeZip 'folder/Книга.fb2' 0 'second.fb2' 1 (Join-Path $root 'unicode.txt') ($normalPaths -join '|')
	if ((Get-Content -LiteralPath (Join-Path $root 'unicode.txt') -Raw) -notmatch '(?m)^menu_count=10$') { throw 'Unified MRU did not keep exactly ten newest records after a mixed >10 session.' }
	$firstCollision = Join-Path $root 'A\Books\archive.zip'; $secondCollision = Join-Path $root 'B\Books\archive.zip'; New-Item -ItemType Directory -Force -Path (Split-Path $firstCollision), (Split-Path $secondCollision) | Out-Null; New-Zip $firstCollision ([object[]]@([object[]]@('book.fb2', $book), [object[]]@('second.fb2', $book))); New-Zip $secondCollision ([object[]]@([object[]]@('book.fb2', $book), [object[]]@('second.fb2', $book))); Invoke-Mru $firstCollision 'book.fb2' 0 'second.fb2' 1 (Join-Path $root 'collision-a.txt'); Invoke-Mru $secondCollision 'book.fb2' 0 'second.fb2' 1 (Join-Path $root 'collision-b.txt')
	$restartReport = Join-Path $root 'restart.txt'; $restartVariables = 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH','FBE_NEXT_TEST_ARCHIVE_ENTRY','FBE_NEXT_TEST_ARCHIVE_OCCURRENCE'; $restartSaved = @{}; foreach($name in $restartVariables) { $restartSaved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
	try { $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'archive-mru-restart-runtime'; $env:FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH = $secondCollision; $env:FBE_NEXT_TEST_ARCHIVE_ENTRY = 'book.fb2'; $env:FBE_NEXT_TEST_ARCHIVE_OCCURRENCE = '0'; $process = Start-Process -FilePath $FbeExe -ArgumentList ('--portable -b {0}' -f (Quote $restartReport)) -WorkingDirectory (Split-Path $FbeExe) -PassThru; $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'archive-mru-restart-runtime' -Report $restartReport -TimeoutSeconds $TimeoutSeconds; Assert-MruRestart $exitCode $restartReport } finally { foreach($name in $restartVariables) { [Environment]::SetEnvironmentVariable($name, $restartSaved[$name], 'Process') } }
    $duplicates = Join-Path $root 'duplicates.zip'; New-Zip $duplicates ([object[]]@([object[]]@('duplicate.fb2', $book), [object[]]@('duplicate.fb2', $book))); Invoke-Mru $duplicates 'duplicate.fb2' 0 'duplicate.fb2' 1 (Join-Path $root 'duplicate.txt')
    $mru = Get-ChildItem -LiteralPath $data -Recurse -Filter ArchiveMRU.txt | Select-Object -First 1
    if($null -eq $mru) { throw 'Versioned archive MRU file was not persisted.' }
    $persisted = Get-Content -LiteralPath $mru.FullName -Raw -Encoding Unicode
    if($persisted -notmatch 'FBE-ARCHIVE-MRU\t2' -or ([regex]::Matches($persisted, 'duplicate\.fb2')).Count -ne 2 -or $persisted -notmatch 'folder/Книга\.fb2') { throw 'Archive MRU persistence lost independent or Unicode identities.' }
    Write-Host 'FBE.exe archive MRU runtime regression passed.'
    $passed = $true
} finally { if ($isolation) { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed ([bool]$passed) }; Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
