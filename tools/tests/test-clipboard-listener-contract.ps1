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
if($source.IndexOf('void CMainFrame::UpdateClipboardCommands()', [StringComparison]::Ordinal) -lt 0) { throw 'Clipboard updates must use a dedicated command path.' }
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('BitmapInClipboard()', [StringComparison]::Ordinal) -ge 0) { throw 'OnIdle must not poll BitmapInClipboard directly.' }
if($idle.IndexOf('RefreshClipboardStateFallbackIfDue();', [StringComparison]::Ordinal) -gt $idle.IndexOf('if (IsSourceActive())', [StringComparison]::Ordinal)) { throw 'Clipboard fallback must run before the BODY/SOURCE-specific idle branches.' }
if($source.IndexOf('if (RefreshClipboardState())', [StringComparison]::Ordinal) -lt 0 -or $source.IndexOf('InvalidateUi(UiDirtyClipboard | UiDirtyToolbar)', [StringComparison]::Ordinal) -lt 0) { throw 'Clipboard fallback must invalidate paste UI only after observed clipboard state changes.' }
if($header.IndexOf('OpenClipboard', [StringComparison]::Ordinal) -ge 0) { throw 'Clipboard bitmap availability must use the non-blocking format query.' }
if($idle -match 'UiDirtyClipboard \| UiDirtyToolbar \| UiDirtyStatus') { throw 'Clipboard, toolbar and status dirty flags must not be a full command-matrix trigger.' }
Write-Host 'Clipboard listener contract passed.'
