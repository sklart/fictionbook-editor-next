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
Require $serializer 'm_sFile + L".tmp"' 'Settings/Hotkeys temporary file'
Require $serializer 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'Settings/Hotkeys atomic replacement'
$settings = Text 'src\fbe\Settings.cpp'
Require $settings 'fileName + L".tmp"' 'Words temporary file'
Require $settings 'MoveFileExW(temporaryFile, fileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)' 'Words atomic replacement'
$locale = Text 'src\fbe\RuntimeLocalization.cpp'
Require $locale 'temporaryPath += L".tmp"' 'Locale temporary file'
Require $locale 'MoveFileExW(temporaryPath, localePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)' 'Locale atomic replacement'
$mru = Text 'src\fbe\mainfrm.cpp'
Require $mru 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' 'MRU atomic replacement'

Write-Host 'Portable atomic persistence contract passed.'
