$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('enum UiDirtyFlags', 'UiDirtySelection', 'UiDirtyClipboard', 'UiDirtySource', 'UiDirtyView', 'UiDirtyToolbar', 'm_ui_dirty', 'InvalidateUi')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "UI dirty-state contract is missing '$token'." }
}
foreach($token in @('UiDirtySource | UiDirtyView | UiDirtyClipboard', 'UiDirtySelection | UiDirtyDocument | UiDirtyView | UiDirtyClipboard', 'm_ui_dirty = UiDirtyNone')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "OnIdle dirty-state gate is missing '$token'." }
}
Write-Host 'UI dirty-state contract passed.'
