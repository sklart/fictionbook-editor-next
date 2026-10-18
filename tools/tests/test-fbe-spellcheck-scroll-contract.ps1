$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$messages = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\apputils.h')

foreach($token in @('WM_BODY_SCROLL', 'UiDirtyScroll', 'OnBodyScroll')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "BODY scroll spellcheck contract is missing '$token'." }
}
function Assert-ScrollProducer([string]$source) {
    $producer = [regex]::Match($source, '(?s)void\s+CFBEView::OnScroll\s*\([^)]*\)\s*\{(?<body>.*?)\r?\n\}')
    if (-not $producer.Success) { throw 'BODY scroll producer is missing.' }
    # MSHTML reports the final scroll position. Preserve the existing synchronous
    # notification used by caret/popup handling; spellcheck itself stays on idle.
    if ($producer.Groups['body'].Value -notmatch '(?s)UpdateSearchHighlightsForScroll\s*\(\s*\)\s*;.*?::SendMessage\s*\(\s*\w+\s*,\s*AU::WM_BODY_SCROLL\s*,\s*0\s*,\s*0\s*\)') {
        throw 'BODY scroll producer must refresh highlights and synchronously notify the frame.'
    }
}
Assert-ScrollProducer $view
foreach ($broken in @($view.Replace('AU::WM_BODY_SCROLL', 'AU::WM_NULL'), $view.Replace('UpdateSearchHighlightsForScroll();', '/* missing refresh */'))) {
    $rejected = $false
    try { Assert-ScrollProducer $broken } catch { $rejected = $true }
    if (-not $rejected) { throw 'BODY scroll producer contract accepted a broken notification path.' }
}
if($messages.IndexOf('WM_BODY_SCROLL', [StringComparison]::Ordinal) -lt 0) { throw 'BODY scroll message is missing.' }
foreach($token in @('InvalidateUi(UiDirtyScroll)', 'UiDirtyScroll', 'm_Speller->CheckScroll()', 'm_ui_dirty & ~UiDirtyScroll')) {
    if($frame.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "BODY scroll consumer contract is missing '$token'." }
}
Write-Host 'BODY scroll spellcheck contract passed.'
