[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text -match $pattern) { throw "Unexpected $description." }
}

Assert-Contains $header 'm_last_zero_length_query' 'zero-length query identity'
Assert-Contains $source 'HasSameSearchCriteria\(m_last_zero_length_query, query\)' 'zero-length criteria reset'
Assert-Contains $source 'm_last_zero_length_query\.Direction == query\.Direction' 'zero-length direction reset'
Assert-Contains $source 'CanReuseDocumentSearch\(query, generation\)' 'cached Find Next decision'
Assert-Contains $source 'GetSession\(\)\.IsValidFor\(generation\)' 'cached session generation validation'
Assert-Contains $source 'GetResults\(\)\.IsValidFor\(generation\)' 'cached results generation validation'
Assert-Contains $source 'm_fo\.scope = AU::Search::SearchScope::WholeDocument' 'Replace scope default'
Assert-Contains $source 'm_fo\.unicodeProperties = false' 'Replace UCP default'
Assert-Contains $source 'openingReplace = !m_replace_dlg \|\| !m_replace_dlg->IsValid\(\)' 'Replace reopen detection'
Assert-Contains $source 'm_has_replace_preview = false' 'Replace preview reset on reopen'
Assert-Contains $source 'return DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);' 'native regex Find Next'
Assert-NotContains $source 'DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);\s*/\*\s*Legacy implementation' 'unreachable legacy regexp implementation'
Assert-Contains $source 'CheckReplacementRange' 'production replacement preflight'
Assert-Contains $source 'fbe\.replace\.cross_paragraph' 'clear cross-paragraph replacement error'

$singleReplace = [regex]::Match($source, 'void\s+CFBEView::DoReplace\(\)\s*\{[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::ReplaceAllSearchCore').Value
if ([string]::IsNullOrWhiteSpace($singleReplace)) { throw 'Unable to locate native single Replace path.' }
Assert-Contains $singleReplace 'CheckReplacementRange\(sel, m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?CrossParagraphReplacementError\(\)[\s\S]*?return;' 'single Replace production preflight rejection'
$singleGuard = $singleReplace.IndexOf('CheckReplacementRange(sel, m_fo.fRegexp)')
$singleUndo = $singleReplace.IndexOf('BeginUndoUnit(L"replace")')
if ($singleGuard -lt 0 -or $singleUndo -lt 0 -or $singleGuard -gt $singleUndo) { throw 'Single Replace must reject a cross-paragraph range before opening Undo.' }

$replaceAll = [regex]::Match($source, 'int\s+CFBEView::ReplaceAllSearchCore\(CString\* errorText\)[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::GlobalReplace').Value
if ([string]::IsNullOrWhiteSpace($replaceAll)) { throw 'Unable to locate native Replace All path.' }
Assert-Contains $replaceAll 'CheckReplacementRange\(ranges\[index\], m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?\*errorText = CrossParagraphReplacementError\(\)[\s\S]*?return -1;' 'Replace All production preflight rejection and error'
$allGuard = $replaceAll.IndexOf('CheckReplacementRange(ranges[index], m_fo.fRegexp)')
$allUndo = $replaceAll.IndexOf('BeginUndoUnit(L"replace all")')
if ($allGuard -lt 0 -or $allUndo -lt 0 -or $allGuard -gt $allUndo) { throw 'Replace All must reject a cross-paragraph range before opening Undo.' }

Write-Host 'Native Search Core contracts passed.'
