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
foreach($required in @('BeginUndoUnit(L"Paste")', 'm_enable_paste', 'ClipboardPastePreparer::Prepare', 'IDM_PASTE', 'Normalize(Document()->body)', 'EndUndoUnit')) {
    if(-not $paste.Contains($required)) { throw "OnPaste lost editor orchestration: $required" }
}
Write-Host 'Clipboard paste boundary passed.'
