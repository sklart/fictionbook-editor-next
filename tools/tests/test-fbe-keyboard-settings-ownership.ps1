<#
.SYNOPSIS
Проверяет перенос настроек раскладки клавиатуры на страницу горячих клавиш.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hotkeys = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsHotkeysDlg.cpp')

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

foreach ($ownership in @(
    @{ Setter = 'SetInsImageAsking'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetIsInsClearImage'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetImageType'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetJpegQuality'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetImageImportFormat'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetImageImportJpegQuality'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetImageImportKeepSupported'; Writer = 'SettingsImagesPage.cpp' },
    @{ Setter = 'SetDefaultEncoding'; Writer = 'SettingsGeneralPage.cpp' },
    @{ Setter = 'SetKeepEncoding'; Writer = 'SettingsGeneralPage.cpp' },
    @{ Setter = 'SetRestoreFilePosition'; Writer = 'SettingsGeneralPage.cpp' },
    @{ Setter = 'SetNBSPChar'; Writer = 'SettingsEditorPage.cpp' }
)) {
    $writers = @(
        Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src\fbe') -Filter 'Settings*.cpp' |
            Where-Object {
                (Get-Content -Raw -LiteralPath $_.FullName) -match ('_Settings\.' + $ownership.Setter + '\s*\(')
            }
    )
    if ($writers.Count -ne 1 -or $writers[0].Name -ne $ownership.Writer) {
        $writerNames = ($writers.Name -join ', ')
        throw "$($ownership.Setter) must have exactly one Settings UI writer, $($ownership.Writer); found: $writerNames"
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
}
if ($hotkeys -notmatch 'GetKeyboardLayoutList\(0, NULL\)' -or $hotkeys -match 'HKL\s+layouts\s*\[\s*16\s*\]') {
    throw 'Keyboard layouts must be enumerated dynamically.'
}
if ($hotkeys -notmatch 'SetItemData\(item, klid\)') {
    throw 'Keyboard UI must preserve a concrete layout identifier.'
}

Write-Host 'Keyboard-layout settings ownership passed.'
