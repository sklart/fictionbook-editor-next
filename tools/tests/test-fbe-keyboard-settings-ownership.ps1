<#
.SYNOPSIS
Проверяет перенос настроек раскладки клавиатуры на страницу горячих клавиш.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hotkeys = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsHotkeysDlg.cpp')
$other = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsOtherDlg.cpp')

foreach ($setter in @('SetChangeKeybLayout', 'SetKeybLayout')) {
    $writers = @(
        Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src\fbe') -Filter 'Settings*.cpp' |
            Where-Object {
                (Get-Content -Raw -LiteralPath $_.FullName) -match ('_Settings\.' + $setter + '\s*\(')
            }
    )
    if ($writers.Count -ne 1 -or $writers[0].Name -ne 'SettingsHotkeysDlg.cpp') {
        $writerNames = ($writers.Name -join ', ')
        throw "$setter must have exactly one Settings UI writer, SettingsHotkeysDlg.cpp; found: $writerNames"
    }
}

foreach ($pattern in @(
    'IDC_SETTINGS_OTHER_KEYBOARD',
    'IDC_CHANGE_KEYB',
    'IDC_SETTINGS_OTHER_CHANGE_TO',
    'IDC_KEYB_LAYOUT',
    'GetChangeKeybLayout\(\)',
    'GetKeybLayout\(\)',
    'SetChangeKeybLayout\(',
    'SetKeybLayout\('
)) {
    if ($hotkeys -notmatch $pattern) {
        throw "SettingsHotkeysDlg.cpp does not own required keyboard-layout behavior: $pattern"
    }
    if ($other -match $pattern) {
        throw "SettingsOtherDlg.cpp retains obsolete keyboard-layout behavior: $pattern"
    }
}

Write-Host 'Keyboard-layout settings ownership passed.'
