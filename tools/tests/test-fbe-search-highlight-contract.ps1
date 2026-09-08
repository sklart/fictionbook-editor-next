[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

Assert-Contains $header 'CSearchHighlightOverlay\*\s+m_search_highlight_overlay' 'overlay ownership boundary'
Assert-Contains $source 'class\s+CSearchHighlightOverlay' 'native highlight overlay'
Assert-Contains $source 'WS_EX_LAYERED\s*\|\s*WS_EX_TRANSPARENT' 'non-interactive transparent overlay'
Assert-Contains $source 'LWA_COLORKEY' 'colour-key transparency'
Assert-Contains $source 'FBE_SEARCH_HIGHLIGHT_TIMER' 'scroll refresh timer'
Assert-Contains $source 'CreateResultRange\(' 'Search Core result-to-range mapping'
Assert-Contains $source 'ClearSearchHighlights\(\);\s*\r?\n\s*if \(!m_ignore_changes\)' 'highlight invalidation on editor mutation'

$overlay = [regex]::Match($source, 'class\s+CSearchHighlightOverlay.*?^};', [Text.RegularExpressions.RegexOptions]::Singleline).Value
if ($overlay -match 'execCommand|innerHTML|outerHTML|\.text\s*=') {
    throw 'Search highlight overlay must not mutate the Design-mode DOM.'
}

Write-Host 'Native Search Highlight All contract test passed.'
