<# Verifies atomic publication for every portable mutable persistence path. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function Require([string]$text, [string]$fragment, [string]$what) {
    if ($text.IndexOf($fragment, [StringComparison]::Ordinal) -lt 0) { throw "${what}: missing '$fragment'." }
}

$serializer = Text 'src\fbe\XMLSerializer\XMLSerializer.cpp'
Require $serializer 'SettingsFileTransaction::Temporary(m_sFile)' 'Settings/Hotkeys unique temporary file'
Require $serializer 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'Settings/Hotkeys atomic replacement'
$wordsStore = Text 'src\fbe\settings\words\WordsStore.cpp'
Require $wordsStore 'SettingsFileTransaction::Temporary(path)' 'Words unique temporary file'
Require $wordsStore 'transaction.Add(path, temporary)' 'Words staged destination'
Require $wordsStore 'SaveToFile(words, temporary) && transaction.Commit()' 'Words publication only after successful save'
$transaction = Text 'src\fbe\settings\SettingsFileTransaction.h'
Require $transaction 'PathRemoveFileSpecW' 'Temporary file must use the destination directory'
Require $transaction 'GetTempFileNameW(directory' 'Unique temporary file allocation'
Require $transaction 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'Transactional replacement and rollback'
Require $transaction 'MoveStagedFile(entry.temporary, entry.destination, MOVEFILE_WRITE_THROUGH)' 'Publish staged Words file'
Require $transaction '::MoveFileExW(source, destination, flags)' 'Bounded publication must retain the native atomic move'
$locale = Text 'src\fbe\RuntimeLocalization.cpp'
Require $locale 'temporaryPath += L".tmp"' 'Locale temporary file'
Require $locale 'MoveFileExW(temporaryPath, localePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)' 'Locale atomic replacement'
$mru = Text 'src\fbe\document\recent\RecentDocumentsStore.cpp'
Require $mru 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'MRU atomic replacement'

Write-Host 'Portable atomic persistence contract passed.'
