[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$searchReplace = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SearchReplace.h')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text -match $pattern) { throw "Unexpected $description." }
}

Assert-Contains $header 'CSearchHighlightOverlay\*\s+m_search_highlight_overlay' 'overlay ownership boundary'
Assert-Contains $source 'class\s+CSearchHighlightOverlay' 'native highlight overlay'
Assert-Contains $source 'WS_POPUP\s*\|\s*WS_DISABLED' 'Win7-compatible owned popup overlay'
Assert-Contains $source 'WS_EX_LAYERED\s*\|\s*WS_EX_TRANSPARENT' 'non-interactive transparent popup overlay'
Assert-Contains $source 'LWA_COLORKEY' 'colour-key transparency'
Assert-Contains $source 'CreateResultRange\(' 'Search Core result-to-range mapping'
Assert-Contains $source 'FindFirstAtOrAfter\(viewportStart\)' 'viewport-limited result lookup'
Assert-Contains $source 'maxOverlayRects\s*=\s*128' 'bounded viewport geometry limit'
Assert-NotContains $source 'FindResultCount\(\)\s*>\s*maxOverlayRects' 'global result-count overlay cutoff'
Assert-Contains $source 'AdvanceSearchDocumentGeneration\(\);\s*\r?\n\s*if \(!m_ignore_changes\)' 'semantic search invalidation on editor mutation'
Assert-Contains $header 'DIID_FBEHTMLElementEvents2, DISPID_HTMLELEMENTEVENTS2_ONSCROLL, OnScroll, &VoidEventInfo' 'post-scroll MSHTML element event sink'
Assert-Contains $source 'void CFBEView::OnScroll\(IDispatch \*/\* unused: evt \*/\)\s*\{[\s\S]*?UpdateSearchHighlightsForScroll\(\);' 'post-scroll overlay refresh'
Assert-NotContains $header 'OnSearchHighlightScroll|WM_MOUSEWHEEL|WM_VSCROLL|WM_HSCROLL' 'pre-scroll Win32 overlay hooks'
$scrollCallCount = [regex]::Matches($source, 'UpdateSearchHighlightsForScroll\(\);').Count
if ($scrollCallCount -ne 1) { throw "Expected one post-scroll overlay refresh call, got $scrollCallCount." }

Assert-Contains $searchReplace 'SetItemText\(item, 1, m_view->FindResultPreview\(index\)\)' 'results preview column'
Assert-NotContains $searchReplace 'FindResultSection|IDD_FIND_RESULTS, L"Section"' 'disabled unsafe results section column'

$overlay = [regex]::Match($source, 'class\s+CSearchHighlightOverlay\s*:\s*public.*?^};', [Text.RegularExpressions.RegexOptions]::Singleline -bor [Text.RegularExpressions.RegexOptions]::Multiline).Value
if ([string]::IsNullOrWhiteSpace($overlay)) {
    throw 'Unable to locate the native highlight overlay implementation.'
}
if ($overlay -match 'execCommand|innerHTML|outerHTML|\.text\s*=') {
    throw 'Search highlight overlay must not mutate the Design-mode DOM.'
}
if ($overlay -match 'WS_CHILD|FBE_SEARCH_HIGHLIGHT_TIMER') {
    throw 'Windows 7 Highlight All must not use a layered child window or polling timer.'
}
if ($overlay -notmatch '(?s)RGB\(255,\s*128,\s*0\).*RGB\(255,\s*215,\s*0\)') {
    throw 'The current hit must be rendered distinctly from other hits.'
}

Write-Host 'Native Search Highlight All contract test passed.'
