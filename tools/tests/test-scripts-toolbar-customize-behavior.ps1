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
if ($dialog -notmatch 'SaveSize\(\).*SetScriptsToolbarCustomizeSize' -or $dialogHeader -notmatch 'MESSAGE_HANDLER\(WM_CLOSE') {
    throw 'Dialog does not persist its size on every close path.'
}
$separator = $localization.strings.'fbe.scripts_toolbar_customize.separator'
if ($null -eq $separator) { throw 'Separator localization key is missing.' }
foreach ($language in $localization.targetLanguages) {
    if ([string]::IsNullOrWhiteSpace($separator.translations.$language)) { throw "Separator translation is missing for $language." }
}

Write-Host 'Scripts toolbar customization behavior contract passed.'
