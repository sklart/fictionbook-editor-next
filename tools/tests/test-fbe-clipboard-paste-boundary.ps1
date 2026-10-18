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
foreach($required in @('ResolveNormalizationScope', 'int beginChar = 0', 'int endChar = 0', 'GetSelectionInfo(std::addressof(selectionBegin), std::addressof(selectionEnd), &beginChar, &endChar', 'FindNormalizationOwner(selectionBegin)', 'FindNormalizationOwner(selectionEnd)', 'beginOwner == endOwner', 'normalization-full', 'normalization-scoped')) {
    if(-not $view.Contains($required)) { throw "Scoped paste normalization is missing: $required" }
}
$scope = [regex]::Match($view, 'MSHTML::IHTMLDOMNodePtr CFBEView::ResolveNormalizationScope\(\)[\s\S]*?(?=void CFBEView::NormalizeScope)').Value
if(-not $scope -or $scope -match 'SelectionContainer\(\)') { throw 'Paste scope must be resolved from both selection endpoints, not the post-paste caret.' }
if($scope -match 'GetSelectionInfo\([^\r\n]*NULL\s*,\s*NULL') { throw 'ResolveNormalizationScope must provide begin/end character outputs to GetSelectionInfo.' }
if($scope -notmatch 'return MSHTML::IHTMLDOMNodePtr\(Document\(\) \? Document\(\)->body : NULL\)') { throw 'Cross-owner or indeterminate paste must fall back to fbw_body.' }
if($view.Contains('RemovePreparedBitmap')) { throw 'CFBEView retains manual temporary bitmap cleanup.' }

$normalize = [regex]::Match($view, 'HRESULT CFBEView::NormalizeWithoutUndo\([^\r\n]*\)[\s\S]*?(?=void CFBEView::Normalize\()').Value
if(-not $normalize) { throw 'Fallible normalization boundary is missing.' }
foreach($required in @('copy(original->cloneNode(VARIANT_TRUE))', 'MSHTML::IHTMLDOMNodePtr copyScope(copy)', 'CheckError(recording.Close())', 'if (!blocked)', 'OpenDropUndoGroup(liveManager, liveParent)', 'CloseDropUndoGroup(liveManager, liveParent, true)', 'raw->RetainOriginalChildren()', 'RollbackRecorded(liveManager)', 'return result')) {
    if(-not $normalize.Contains($required)) { throw "Normalization lost preparation/transaction boundary: $required" }
}
foreach($operation in @('RemoveUnk', 'MergeEqualHTMLElements', 'FbeVisualDom::NormalizeStructure', 'FixupLinks')) {
    $copyCall = [regex]::Match($normalize, [regex]::Escape($operation) + '\((?:Document\(\),\s*)?copyScope(?:,\s*Document\(\))?\)')
    $liveCall = [regex]::Match($normalize, [regex]::Escape($operation) + '\((?:Document\(\),\s*)?transform(?:,\s*Document\(\))?\)')
    if(-not $copyCall.Success -or -not $liveCall.Success -or $copyCall.Index -ge $liveCall.Index) { throw "Normalization must validate $operation on the orphan copy before the native live pass." }
    if($liveCall.Index -le $normalize.IndexOf('FbeDom::MarkupUndoUnitScope native')) { throw "Live normalization escaped native undo: $operation" }
}
if($view -notmatch 'void CFBEView::NormalizeScope\([^\r\n]*\)[\s\S]*?CheckError\(NormalizeWithoutUndo\(dom\)\)') { throw 'Paste ignores normalization failure.' }
foreach($required in @('OpenDropUndoGroup(pasteManager, pasteParent)', 'transaction->PrepareRollback(pasteManager)', 'FbeDom::MarkupUndoUnitScope native(m_mk_srv, L"Paste HTML")', 'transaction->FinishPaste()', 'CloseDropUndoGroup(pasteManager, pasteParent, true)', 'RollbackRecorded(pasteManager)')) {
    if(-not $paste.Contains($required)) { throw "HTML paste lost native transaction: $required" }
}
foreach($required in @('ClipboardPastePreparationResult::~ClipboardPastePreparationResult()', 'RemovePreparedBitmap();', 'ClipboardPastePreparationResult&& other', 'image.Detach()', '::GlobalUnlock', '::CloseClipboard()', '::DeleteFile')) {
    if(-not $preparerSource.Contains($required)) { throw "Clipboard preparer lost resource ownership: $required" }
}
$preparerPrepare = $paste.IndexOf('ClipboardPastePreparer::Prepare')
if($preparerPrepare -ge $paste.LastIndexOf('IDM_PASTE') -or $paste.LastIndexOf('IDM_PASTE') -ge $paste.LastIndexOf('NormalizeScope(normalizationScope)')) { throw 'Legacy preparation, native paste and normalization are out of order.' }
Write-Host 'Clipboard paste boundary passed.'
