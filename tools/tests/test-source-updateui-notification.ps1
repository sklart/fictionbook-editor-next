[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$handler = [regex]::Match($source, 'LRESULT\s+OnSciUpdateUI\s*\([^)]*\)\s*\{[\s\S]*?(?=\n\s*LRESULT\s+OnGoToMatchTag)')
if (!$handler.Success) { throw 'OnSciUpdateUI implementation was not found.' }

foreach ($required in @(
    'hdr->hwndFrom',
    'm_source',
    'm_current_view != SOURCE',
    'SC_UPDATE_LINE_COUNT',
    'SC_UPDATE_SELECTION',
    'SC_UPDATE_TEXT',
    'UpdateSourceLineNumberMargin(false)',
    'SciUpdateUI(false)'
)) {
    if ($handler.Value -notlike "*$required*") {
        throw "OnSciUpdateUI is missing required behavior: $required"
    }
}

if ($handler.Value -match 'if\s*\(\s*m_current_view\s*==\s*SOURCE\s*\)\s*SciUpdateUI\s*\(\s*false\s*\)') {
    throw 'OnSciUpdateUI must not refresh matching tags for every SCN_UPDATEUI notification.'
}

if ($handler.Value -notmatch 'scn\.updated\s*&\s*\(\s*SC_UPDATE_SELECTION\s*\|\s*SC_UPDATE_TEXT\s*\)') {
    throw 'OnSciUpdateUI must refresh matching tags only for selection or text updates.'
}

Write-Host 'Source SCN_UPDATEUI notification contract passed.'
