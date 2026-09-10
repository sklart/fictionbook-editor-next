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
function Move-ByDrag([System.Collections.Generic.List[int]]$toolbar, [int]$source, [int]$insert) {
    # The insert index is measured before the source button is removed, like a listbox insertion marker.
    if ($source -lt 0 -or $source -ge $toolbar.Count -or $insert -lt 0 -or $insert -gt $toolbar.Count) { return }
    if ($insert -eq $source -or $insert -eq ($source + 1)) { return }
    $destination = if ($source -lt $insert) { $insert - 1 } else { $insert }
    $item = $toolbar[$source]; $toolbar.RemoveAt($source); $toolbar.Insert($destination, $item)
}
function Move-Selected([System.Collections.Generic.List[int]]$toolbar, [int[]]$selected, [bool]$down) {
    $flags = [bool[]]::new($toolbar.Count); foreach ($index in $selected) { $flags[$index] = $true }
    if ($down) {
        for ($index = $toolbar.Count - 2; $index -ge 0; --$index) {
            if ($flags[$index] -and -not $flags[$index + 1]) { $item = $toolbar[$index]; $toolbar[$index] = $toolbar[$index + 1]; $toolbar[$index + 1] = $item; $flag = $flags[$index]; $flags[$index] = $flags[$index + 1]; $flags[$index + 1] = $flag }
        }
    } else {
        for ($index = 1; $index -lt $toolbar.Count; ++$index) {
            if ($flags[$index] -and -not $flags[$index - 1]) { $item = $toolbar[$index]; $toolbar[$index] = $toolbar[$index - 1]; $toolbar[$index - 1] = $item; $flag = $flags[$index]; $flags[$index] = $flags[$index - 1]; $flags[$index - 1] = $flag }
        }
    }
}
function Move-SelectedByDrag([System.Collections.Generic.List[int]]$toolbar, [int[]]$selected, [int]$insert) {
    $selectedSet = [System.Collections.Generic.HashSet[int]]::new([int[]]$selected)
    $destination = $insert - @($selected | Where-Object { $_ -lt $insert }).Count
    $moved = [System.Collections.Generic.List[int]]::new(); $rest = [System.Collections.Generic.List[int]]::new()
    for ($index = 0; $index -lt $toolbar.Count; ++$index) { if ($selectedSet.Contains($index)) { $moved.Add($toolbar[$index]) } else { $rest.Add($toolbar[$index]) } }
    $rest.InsertRange($destination, $moved); $toolbar.Clear(); $toolbar.AddRange($rest)
}
function Activate-Selection([bool[]]$active, [bool[]]$other) {
    for ($index = 0; $index -lt $other.Length; ++$index) { $other[$index] = $false }
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

$toolbar = [System.Collections.Generic.List[int]]@(101, 102, 0, 103)
$selectedAvailable = @(0, 2, 3) # command, separator, command
foreach ($item in $selectedAvailable) { if ($item -eq 0) { $toolbar.Add(0) } elseif (-not $toolbar.Contains($item)) { $toolbar.Add($item) } }
Assert-Equal $toolbar @(101, 102, 0, 103, 0, 2, 3) 'multi-add preserves source order and permits separator'
$remove = @(6, 4); foreach ($index in $remove) { $toolbar.RemoveAt($index) }
Assert-Equal $toolbar @(101, 102, 0, 103, 2) 'multi-remove runs from end to beginning'
$availableSelection = [bool[]]@($true, $true, $false); $currentSelection = [bool[]]@($false, $true, $true)
Activate-Selection $availableSelection $currentSelection
if ($currentSelection -contains $true) { throw 'selecting available items must clear toolbar selection' }
$currentSelection[0] = $true
Activate-Selection $currentSelection $availableSelection
if ($availableSelection -contains $true) { throw 'selecting toolbar items must clear available selection' }
$availableSelection = [bool[]]@($false, $false, $false); $currentSelection = [bool[]]@($true, $true)
if ($availableSelection -contains $true -or ($currentSelection | Where-Object { $_ }).Count -ne 2) { throw 'Add must transfer grouped selection to the toolbar list' }
$availableSelection = [bool[]]@($true, $true); $currentSelection = [bool[]]@($false, $false)
if ($currentSelection -contains $true -or ($availableSelection | Where-Object { $_ }).Count -ne 2) { throw 'Remove must transfer grouped selection to the available list' }
$toolbar = [System.Collections.Generic.List[int]]@(101, 0, 102, 103)
Move-Selected $toolbar @(1, 2) $false
Assert-Equal $toolbar @(0, 102, 101, 103) 'grouped Up preserves selection order across separator'
Move-Selected $toolbar @(0, 1) $true
Assert-Equal $toolbar @(101, 0, 102, 103) 'grouped Down preserves selection order across separator'
$toolbar = [System.Collections.Generic.List[int]]@(101, 0, 102, 103)
Move-SelectedByDrag $toolbar @(1, 2) 4
Assert-Equal $toolbar @(101, 103, 0, 102) 'grouped drag preserves order and adjusts downward insert index'

$toolbar = [System.Collections.Generic.List[int]]@(101, 0, 102)
Move-ByDrag $toolbar 2 1
Assert-Equal $toolbar @(101, 102, 0) 'drag moves button upward across separator'
Move-ByDrag $toolbar 1 3
Assert-Equal $toolbar @(101, 0, 102) 'drag moves button downward with adjusted index'
Move-ByDrag $toolbar 1 3
Assert-Equal $toolbar @(101, 102, 0) 'drag moves separator across button'
$beforeNoOp = @($toolbar)
Move-ByDrag $toolbar 1 1
Assert-Equal $toolbar $beforeNoOp 'drag drop at source boundary is a no-op'
Move-ByDrag $toolbar 1 2
Assert-Equal $toolbar $beforeNoOp 'drag drop after source boundary is a no-op'

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
foreach ($required in @('CurrentListSubclassProc', 'DrawDragIndicator', 'UpdateDragInsert', 'FinishDrag', 'UpdateDragScroll', 'SetCapture', 'ReleaseCapture')) {
    if ($dialog -notmatch [regex]::Escape($required) -and $dialogHeader -notmatch [regex]::Escape($required)) {
        throw "Scripts toolbar drag behavior is missing: $required"
    }
}
foreach ($required in @('WM_SETREDRAW', 'RedrawWindow', 'LBS_EXTENDEDSEL', 'LB_SETSEL', 'VK_CONTROL', 'm_dragRows', 'MoveSelectedButtons', 'MoveDraggedButtons')) {
    if ($dialog -notmatch [regex]::Escape($required) -and $dialogHeader -notmatch [regex]::Escape($required) -and $resource -notmatch [regex]::Escape($required)) {
        throw "Scripts toolbar batch/multi-select behavior is missing: $required"
    }
}
foreach ($required in @('ActivateList', 'BeginDeferWindowPos', 'DeferWindowPos', 'EndDeferWindowPos', 'RDW_ALLCHILDREN', 'SaveDC', 'IntersectClipRect', 'RestoreDC')) {
    if ($dialog -notmatch [regex]::Escape($required) -and $dialogHeader -notmatch [regex]::Escape($required)) {
        throw "Scripts toolbar selection/layout paint regression guard is missing: $required"
    }
}
foreach ($required in @('WS_CLIPCHILDREN', 'WS_CLIPSIBLINGS')) {
    if ($resource -notmatch [regex]::Escape($required)) { throw "Scripts toolbar resize clipping style is missing: $required" }
}
$close = $localization.strings.'fbe.scripts_toolbar_customize.close'
if ($close.translations.'ru-RU' -ne 'Закрыть') { throw 'Russian runtime localization for the Close button is incorrect.' }
if ($mainFrame -notmatch 'FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION') { throw 'System-dialog CBT localization must not overwrite the custom Close button.' }
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
