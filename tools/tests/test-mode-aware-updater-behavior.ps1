<# Exercises the Modern portable update selection without downloading anything. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\AboutBox.cpp')

# The production selector is intentionally a small pure mode/profile decision:
# Modern + Portable must request its ZIP and cannot take the setup branch.
$modernPortable = 'FictionBookEditorNext-%s-win32-portable.zip'
$modernSetup = 'FictionBookEditorNext-%s-win32-setup.exe'
if ($source.IndexOf($modernPortable, [StringComparison]::Ordinal) -lt 0) { throw 'Modern portable artifact name is missing.' }
if ($source.IndexOf($modernSetup, [StringComparison]::Ordinal) -lt 0) { throw 'Modern setup artifact name is missing.' }
if ($source -notmatch 'portable\s*\?\s*\(win7 \?[^\r\n]+win7-win32-portable\.zip"\s*:\s*L"%sv%s/FictionBookEditorNext-%s-win32-portable\.zip"\)') {
    throw 'Modern portable mode does not select the portable ZIP branch.'
}
if ($source -notmatch 'portable \? L"\.zip" : L"\.exe"') { throw 'Portable updater does not reject setup.exe artifacts.' }
Write-Host 'Modern portable updater behavioral selection passed (no download).'
