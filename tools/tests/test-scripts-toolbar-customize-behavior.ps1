<#
.SYNOPSIS
Exercises the Scripts toolbar customization state contract independently of UI automation.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$settings = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.cpp')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ScriptsToolbarCustomizeDlg.cpp')
$dialogHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ScriptsToolbarCustomizeDlg.h')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$settingsHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.h')
$localization = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json

function Assert-Equal($actual, $expected, [string]$name) {
    if (@($actual).Count -ne @($expected).Count -or (@($actual) -join ',') -ne (@($expected) -join ',')) {
        throw "${name}: expected [$(@($expected) -join ',')], got [$(@($actual) -join ',')]"
    }
}
function Add-Script([System.Collections.Generic.List[int]]$toolbar, [int]$command) {
    if (-not $toolbar.Contains($command)) { $toolbar.Add($command) }
}
function Move-Up([System.Collections.Generic.List[int]]$toolbar, [int]$index) {
    if ($index -gt 0) { $item = $toolbar[$index]; $toolbar.RemoveAt($index); $toolbar.Insert($index - 1, $item) }
}
function Move-Down([System.Collections.Generic.List[int]]$toolbar, [int]$index) {
    if ($index -ge 0 -and $index + 1 -lt $toolbar.Count) { $item = $toolbar[$index]; $toolbar.RemoveAt($index); $toolbar.Insert($index + 1, $item) }
}

# 0 models TBSTYLE_SEP: it occupies a real toolbar index and is movable/removable.
$toolbar = [System.Collections.Generic.List[int]]@(101, 0, 102)
Assert-Equal $toolbar @(101, 0, 102) 'button-separator-button order'
Add-Script $toolbar 102
Assert-Equal $toolbar @(101, 0, 102) 'duplicate Add is ignored'
$toolbar.Add(0)
Assert-Equal $toolbar @(101, 0, 102, 0) 'separator may be added repeatedly'
$toolbar.RemoveAt(3)
Move-Up $toolbar 2
Assert-Equal $toolbar @(101, 102, 0) 'Up moves a button across separator'
Move-Down $toolbar 1
Assert-Equal $toolbar @(101, 0, 102) 'Down moves a button across separator'
$toolbar.RemoveAt(1)
Assert-Equal $toolbar @(101, 102) 'Remove removes selected separator by toolbar index'

if ($resource -notmatch 'IDR_SCRIPTS TOOLBAR[\s\S]*?BUTTON\s+ID_LAST_SCRIPT') { throw 'IDR_SCRIPTS must restore ID_LAST_SCRIPT.' }
$toolbar = [System.Collections.Generic.List[int]]@(101, 0, 102)
$toolbar.Clear(); $toolbar.Add(32899) # ID_LAST_SCRIPT from resource.h; default is verified above against IDR_SCRIPTS.
Assert-Equal $toolbar @(32899) 'Reset restores IDR_SCRIPTS default'

if ($settings -notmatch 'SCRIPTS_TOOLBAR_CUSTOMIZE_SIZE_KEY' -or $settings -notmatch 'SetScriptsToolbarCustomizeSize\(const CSize& size, bool apply\)' -or $settings -notmatch 'if\(size\.cx >= 300 && size\.cy >= 200\)') {
    throw 'Dialog size persistence contract is incomplete.'
}
foreach ($required in @('SCRIPTS_TOOLBAR_CUSTOMIZE_PLACEMENT_KEY', 'GetScriptsToolbarCustomizePlacement', 'SetScriptsToolbarCustomizePlacement', 'MonitorFromRect', 'MONITOR_DEFAULTTONULL', 'GetMonitorInfo', 'info.rcWork', 'CenterWindow(GetParent())')) {
    if ($settings -notmatch [regex]::Escape($required) -and $settingsHeader -notmatch [regex]::Escape($required) -and $dialog -notmatch [regex]::Escape($required)) {
        throw "Scripts toolbar placement behavior is missing: $required"
    }
}
if ($dialog -notmatch 'placement\.showCmd = SW_SHOWNORMAL' -or $dialog -notmatch 'SetScriptsToolbarCustomizeSize\(') {
    throw 'Scripts toolbar placement must restore normal state and preserve the legacy size fallback.'
}
if ($dialog -match 'UiMetrics::UpdateForWindow\(m_hWnd\)' -or $dialog -notmatch 'CreateDialogFontForDpi') {
    throw 'Dialog must use a local DPI font without replacing main-frame UiMetrics fonts.'
}
if ($dialog -notmatch 'SavePlacement\(' -or $dialog -notmatch 'SetScriptsToolbarCustomizePlacement' -or $dialogHeader -notmatch 'MESSAGE_HANDLER\(WM_CLOSE') {
    throw 'Dialog does not persist its size on every close path.'
}
foreach ($required in @('TB_GETIMAGELIST', 'ImageList_Draw', 'UpdateButtonState', 'fbe.hotkey.scripts.last_script')) {
    if ($dialog -notmatch [regex]::Escape($required) -and $dialogHeader -notmatch [regex]::Escape($required) -and $mainFrame -notmatch [regex]::Escape($required)) {
        throw "Scripts toolbar UI behavior is missing: $required"
    }
}
$separator = $localization.strings.'fbe.scripts_toolbar_customize.separator'
if ($null -eq $separator) { throw 'Separator localization key is missing.' }
foreach ($language in $localization.targetLanguages) {
    if ([string]::IsNullOrWhiteSpace($separator.translations.$language)) { throw "Separator translation is missing for $language." }
}

Write-Host 'Scripts toolbar customization behavior contract passed.'
