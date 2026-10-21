<# Guards the dynamic row formula and its DPI/command-toolbar boundary; HWND behavior runs in the lifecycle suite. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src/fbe/ScriptsToolbarCustomizeDlg.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $root 'src/fbe/mainfrm.cpp')
$formula = 'm_showPanelSelector ? (std::max)(28, m_settings.GetScriptIconSize() + 8) : 28'
foreach ($required in @(
    "itemHeight = Scale($formula)",
    "const int rowHeight = Scale($formula)",
    'if(m_showPanelSelector) {',
    'const int rowHeight = Scale((std::max)(28, m_settings.GetScriptIconSize() + 8));',
    'IDC_SCRIPTS_TOOLBAR_AVAILABLE), LB_SETITEMHEIGHT, 0, rowHeight',
    'IDC_SCRIPTS_TOOLBAR_CURRENT), LB_SETITEMHEIGHT, 0, rowHeight',
    'return UiMetrics::ScaleForDpi(px, m_dpi);',
    'm_dpi = UiMetrics::DpiForWindow(m_hWnd);',
    'LB_GETITEMHEIGHT, 0, 0) == rowHeight'
)) {
    if (-not $dialog.Contains($required)) { throw "Customize row metrics contract missing: $required" }
}
if ($dialog -notmatch '(?s)if\(m_showPanelSelector\) \{\s*const int rowHeight = Scale\(\(std::max\)\(28, m_settings.GetScriptIconSize\(\) \+ 8\)\);\s*::SendMessage\([^;]+LB_SETITEMHEIGHT[^;]+;\s*::SendMessage\([^;]+LB_SETITEMHEIGHT[^;]+;\s*\}') {
    throw 'Only script dialogs may override both list row heights from ScriptIconSize.'
}
if ($frame -notmatch '(?s)dialog\(m_CmdToolbar,.*?UpdateCommandToolbarItems\(items\); \}, false,' -or
    $frame -notmatch '(?s)dialog\(selected,.*?UpdateScriptToolbarItems\(id, items\); \}, true,') {
    throw 'Command and script customize dialogs must select different row sizing policies.'
}
# Expected logical rows: scripts 28/28/32/40; commands always 28.
# MulDiv uses nearest integer rounding. This matrix documents the checked formula,
# while the lifecycle runtime independently checks actual listbox rows and icons.
$expected = @{ 96 = @(28,28,32,40); 120 = @(35,35,40,50); 144 = @(42,42,48,60); 192 = @(56,56,64,80) }
foreach ($dpi in @(96,120,144,192)) {
    $index = 0
    foreach ($icon in @(16,20,24,32)) {
        $scriptRow = [int][Math]::Floor(([Math]::Max(28, $icon + 8) * $dpi + 48) / 96)
        $commandRow = [int][Math]::Floor((28 * $dpi + 48) / 96)
        if ($scriptRow -ne $expected[$dpi][$index] -or $commandRow -ne $expected[$dpi][0]) {
            throw "Incorrect row metrics for icon=$icon dpi=$dpi"
        }
        $index++
    }
}
Write-Host 'Customize row metrics contract passed (16/20/24/32; DPI 100/125/150/200%; commands remain independent).'
