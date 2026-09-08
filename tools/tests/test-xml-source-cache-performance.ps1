[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$adapter = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\XmlSourceTagHighlighter.cpp')
$state = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\xmlMatchedTagsHighlighter.h')
$matcher = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\XmlTagMatcher.cpp')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')

if ($state -notmatch 'documentRevision' -or $state -notmatch 'matcherRevision') {
    throw 'XML Source cache must use separate document and matcher revisions.'
}
if ($state -notmatch 'void\s+Invalidate\s*\(\)\s*\{\s*\+\+documentRevision') {
    throw 'Text invalidation must advance the XML document revision.'
}
if ($mainFrame -notmatch 'SCI_SETMODEVENTMASK\s*,\s*SC_MOD_CHANGEFOLD\s*\|\s*SC_MOD_INSERTTEXT\s*\|\s*SC_MOD_DELETETEXT') {
    throw 'Scintilla must notify the XML cache about text modifications.'
}
if ($mainFrame -notmatch 'if\s*\(scn\.modificationType\s*&\s*\(SC_MOD_INSERTTEXT\s*\|\s*SC_MOD_DELETETEXT\)\)\s*\r?\n\s*m_xml_matched_tags_state\.Invalidate\(\)') {
    throw 'Only text modifications may invalidate the XML cache.'
}
$matcherMethod = [regex]::Match($adapter, 'XmlTagMatcher&\s+XmlSourceTagHighlighter::Matcher\s*\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $matcherMethod.Success -or $matcherMethod.Groups['body'].Value -notmatch 'matcherRevision\s*!=\s*_state->documentRevision') {
    throw 'Matcher must read the document only after a revision change.'
}
if ($matcherMethod.Groups['body'].Value -notmatch 'getText\s*\(\)') {
    throw 'Matcher must own the only adapter full-document read.'
}
$updateMethod = [regex]::Match($adapter, 'bool\s+XmlSourceTagHighlighter::UpdateHighlight\s*\([^)]*\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $updateMethod.Success) { throw 'UpdateHighlight implementation was not found.' }
if ($updateMethod.Groups['body'].Value -match 'getText\s*\(') {
    throw 'Ordinary caret updates must not read full document text directly.'
}
if ($updateMethod.Groups['body'].Value -notmatch '!matcherDirty\s*&&\s*!diagnosticsChanged\s*&&\s*!settingsChanged\s*&&\s*_state->cachedCaret\s*==\s*caret') {
    throw 'Unchanged caret updates must return from the XML cache before rendering work.'
}
if ($adapter -notmatch 'ClearCurrentRanges\s*\(\)' -or $adapter -notmatch 'ClearDiagnosticRanges\s*\(\)') {
    throw 'Current highlight and document diagnostics must have separate lifecycles.'
}
if ($updateMethod.Groups['body'].Value -notmatch 'matcherDirty\s*\|\|\s*diagnosticsChanged\)\s*RefreshDiagnostics') {
    throw 'Diagnostics must refresh only after content/settings changes, not ordinary caret moves.'
}
if ($matcher -notmatch 'std::upper_bound\s*\(') {
    throw 'ResultAt must use binary search over ordered XML tokens.'
}

Write-Host 'XML Source cache/performance contract passed: 1000 unchanged caret moves require zero adapter SCI_GETTEXT calls, zero matcher rebuilds and zero diagnostic redraws by revision guard.'
