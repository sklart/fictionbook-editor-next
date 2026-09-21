$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$messages = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\apputils.h')

foreach($token in @('WM_BODY_SCROLL', 'UiDirtyScroll', 'OnBodyScroll')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "BODY scroll spellcheck contract is missing '$token'." }
}
foreach($token in @('WM_BODY_SCROLL', 'PostMessage(m_frame, AU::WM_BODY_SCROLL')) {
    if($view.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "BODY scroll producer contract is missing '$token'." }
}
if($messages.IndexOf('WM_BODY_SCROLL', [StringComparison]::Ordinal) -lt 0) { throw 'BODY scroll message is missing.' }
foreach($token in @('InvalidateUi(UiDirtyScroll)', 'UiDirtyScroll', 'm_Speller->CheckScroll()', 'm_ui_dirty & ~UiDirtyScroll')) {
    if($frame.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "BODY scroll consumer contract is missing '$token'." }
}
Write-Host 'BODY scroll spellcheck contract passed.'
