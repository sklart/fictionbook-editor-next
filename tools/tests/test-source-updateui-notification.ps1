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

$marginCall = 'UpdateSourceLineNumberMargin\s*\(\s*false\s*\)'
$matchingTagsCall = 'SciUpdateUI\s*\(\s*false\s*\)'
if ([regex]::Matches($handler.Value, $marginCall).Count -ne 1) {
    throw 'OnSciUpdateUI must update the line-number margin exactly once.'
}
if ([regex]::Matches($handler.Value, $matchingTagsCall).Count -ne 1) {
    throw 'OnSciUpdateUI must refresh matching tags exactly once.'
}

$marginCondition = 'if\s*\(\s*scn\.updated\s*&\s*SC_UPDATE_LINE_COUNT\s*\)\s*(?:\{\s*)?'
if ($handler.Value -notmatch ($marginCondition + $marginCall)) {
    throw 'OnSciUpdateUI must update the line-number margin only for SC_UPDATE_LINE_COUNT.'
}

$matchingTagsCondition = 'if\s*\(\s*scn\.updated\s*&\s*\(\s*SC_UPDATE_SELECTION\s*\|\s*SC_UPDATE_TEXT\s*\)\s*\)\s*(?:\{\s*)?'
if ($handler.Value -notmatch ($matchingTagsCondition + $matchingTagsCall)) {
    throw 'OnSciUpdateUI must refresh matching tags only for selection or text updates.'
}

$combinedEarlyExit = 'if\s*\(\s*hdr->hwndFrom\s*!=\s*m_source\s*\|\|\s*m_current_view\s*!=\s*SOURCE\s*\)\s*(?:\{\s*)?return\s+0\s*;'
$sourceEarlyExit = 'if\s*\(\s*hdr->hwndFrom\s*!=\s*m_source\s*\)\s*(?:\{\s*)?return\s+0\s*;'
$viewEarlyExit = 'if\s*\(\s*m_current_view\s*!=\s*SOURCE\s*\)\s*(?:\{\s*)?return\s+0\s*;'
if ($handler.Value -notmatch $combinedEarlyExit -and
    -not ($handler.Value -match $sourceEarlyExit -and $handler.Value -match $viewEarlyExit)) {
    throw 'OnSciUpdateUI must return early for notifications from another control or inactive Source view.'
}

Write-Host 'Source SCN_UPDATEUI notification contract passed.'
