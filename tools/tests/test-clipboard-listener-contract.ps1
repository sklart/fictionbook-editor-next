$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('WM_CLIPBOARDUPDATE', 'm_clipboard_listener_registered', 'm_clipboard_has_bitmap', 'OnClipboardUpdate')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Clipboard state contract is missing '$token'." }
}
foreach($token in @('AddClipboardFormatListener(m_hWnd)', 'RemoveClipboardFormatListener(m_hWnd)', 'RefreshClipboardStateFallbackIfDue()', 'm_clipboard_has_bitmap')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Clipboard implementation is missing '$token'." }
}
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('BitmapInClipboard()', [StringComparison]::Ordinal) -ge 0) { throw 'OnIdle must not poll BitmapInClipboard directly.' }
Write-Host 'Clipboard listener contract passed.'
