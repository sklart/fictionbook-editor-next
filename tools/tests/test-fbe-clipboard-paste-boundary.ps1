<# Guards the Windows clipboard-preparation boundary from editor/DOM ownership. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$preparerHeader = Get-Content -Raw (Join-Path $root 'src\fbe\clipboard\ClipboardPastePreparer.h')
$preparerSource = Get-Content -Raw (Join-Path $root 'src\fbe\clipboard\ClipboardPastePreparer.cpp')
$view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

foreach($forbidden in @('FBEview.h', 'CFBEView', 'mainfrm.h', 'CMainFrame', 'MSHTML', 'IHTMLDocument', 'IHTMLElement', 'IHTMLTxtRange', 'FB::Doc', 'DocumentSession', 'EditorViewController', '_Settings', 'IDM_PASTE')) {
    if($preparerHeader.Contains($forbidden) -or $preparerSource.Contains($forbidden)) {
        throw "ClipboardPastePreparer leaks forbidden dependency: $forbidden"
    }
}

if(-not $view.Contains('ClipboardPastePreparer::Prepare')) {
    throw 'CFBEView::OnPaste does not delegate clipboard preparation.'
}

$start = $view.IndexOf('LRESULT CFBEView::OnPaste', [StringComparison]::Ordinal)
if($start -lt 0) { throw 'CFBEView::OnPaste is missing.' }
$open = $view.IndexOf('{', $start)
$depth = 0; $end = -1
for($index = $open; $index -lt $view.Length; ++$index) {
    if($view[$index] -eq '{') { ++$depth }
    elseif($view[$index] -eq '}') {
        --$depth
        if($depth -eq 0) { $end = $index; break }
    }
}
if($end -lt 0) { throw 'Unable to locate complete OnPaste body.' }
$paste = $view.Substring($open + 1, $end - $open - 1)
foreach($forbidden in @('GetClipboardData', 'SetClipboardData', 'GlobalLock', 'GlobalAlloc', 'GetTempPath', 'GetTempFileName', 'CImage')) {
    if($paste.Contains($forbidden)) { throw "OnPaste retains clipboard preparation: $forbidden" }
}
foreach($required in @('FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"Paste")', 'PasteEnableScope pasteEnabled(m_enable_paste)', 'pasteEnabled.Close()', 'ClipboardPastePreparer::Prepare', 'normalizationScope = ResolveNormalizationScope()', 'IDM_PASTE', 'NormalizeScope(normalizationScope)', 'undo.Close()')) {
    if(-not $paste.Contains($required)) { throw "OnPaste lost editor orchestration: $required" }
}
foreach($required in @('ResolveNormalizationScope', 'int beginChar = 0', 'int endChar = 0', 'GetSelectionInfo(std::addressof(selectionBegin), std::addressof(selectionEnd), &beginChar, &endChar', 'FindNormalizationOwner(selectionBegin)', 'FindNormalizationOwner(selectionEnd)', 'beginOwner == endOwner', 'normalization-full', 'normalization-scoped', 'RemoveUnk(scopeNode,Document())', 'MergeEqualHTMLElements(scopeNode, Document())', 'FbeVisualDom::NormalizeStructure(Document(), scopeNode)', 'FixupLinks(scopeNode)')) {
    if(-not $view.Contains($required)) { throw "Scoped paste normalization is missing: $required" }
}
$scope = [regex]::Match($view, 'MSHTML::IHTMLDOMNodePtr CFBEView::ResolveNormalizationScope\(\)[\s\S]*?(?=void CFBEView::NormalizeScope)').Value
if(-not $scope -or $scope -match 'SelectionContainer\(\)') { throw 'Paste scope must be resolved from both selection endpoints, not the post-paste caret.' }
if($scope -match 'GetSelectionInfo\([^\r\n]*NULL\s*,\s*NULL') { throw 'ResolveNormalizationScope must provide begin/end character outputs to GetSelectionInfo.' }
if($scope -notmatch 'return MSHTML::IHTMLDOMNodePtr\(Document\(\) \? Document\(\)->body : NULL\)') { throw 'Cross-owner or indeterminate paste must fall back to fbw_body.' }
if($view.Contains('RemovePreparedBitmap')) { throw 'CFBEView retains manual temporary bitmap cleanup.' }
Write-Host 'Clipboard paste boundary passed.'
