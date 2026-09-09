[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

Assert-Contains $header 'm_last_zero_length_query' 'zero-length query identity'
Assert-Contains $source 'HasSameSearchCriteria\(m_last_zero_length_query, query\)' 'zero-length criteria reset'
Assert-Contains $source 'm_last_zero_length_query\.Direction == query\.Direction' 'zero-length direction reset'
Assert-Contains $source 'CanReuseDocumentSearch\(query, generation\)' 'cached Find Next decision'
Assert-Contains $source 'GetSession\(\)\.IsValidFor\(generation\)' 'cached session generation validation'
Assert-Contains $source 'GetResults\(\)\.IsValidFor\(generation\)' 'cached results generation validation'
Assert-Contains $source 'm_fo\.scope = AU::Search::SearchScope::WholeDocument' 'Replace scope default'
Assert-Contains $source 'm_fo\.unicodeProperties = false' 'Replace UCP default'
Assert-Contains $source 'return DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);' 'native regex Find Next'
Assert-Contains $source 'IsCrossParagraphReplacementRange' 'structural replacement guard'
Assert-Contains $source 'fbe\.replace\.cross_paragraph' 'clear cross-paragraph replacement error'

Write-Host 'Native Search Core contracts passed.'
