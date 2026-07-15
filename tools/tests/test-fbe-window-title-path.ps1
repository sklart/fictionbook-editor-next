<#
.SYNOPSIS
Проверяет настройку полного пути в заголовке окна FBE.

.DESCRIPTION
Проверка подтверждает, что настройка сохраняется в Settings.xml, доступна на
вкладке FBE Next и использует сокращение длинного пути по ширине окна.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$settingsHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.h')
$settingsSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.cpp')
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SettingsNextDlg.cpp')
$frameSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$frameHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')

foreach ($contract in @(
    @{ Text = 'm_show_full_path_in_window_title'; Source = $settingsHeader; Name = 'поле настройки' },
    @{ Text = 'GetShowFullPathInWindowTitle'; Source = $settingsHeader; Name = 'getter настройки' },
    @{ Text = 'SetShowFullPathInWindowTitle'; Source = $settingsHeader; Name = 'setter настройки' },
    @{ Text = 'ShowFullPathInWindowTitle'; Source = $settingsSource; Name = 'ключ Settings.xml' },
    @{ Text = 'm_show_full_path_in_window_title\s*=\s*false'; Source = $settingsSource; Name = 'значение по умолчанию' },
    @{ Text = 'IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE'; Source = $dialogSource; Name = 'контрол вкладки FBE Next' },
    @{ Text = 'U::GetFullPathName'; Source = $frameSource; Name = 'полный путь в заголовке' },
    @{ Text = 'GetTextExtentPoint32W'; Source = $frameSource; Name = 'проверка ширины пути' },
    @{ Text = 'm_need_title_update\s*=\s*true'; Source = $frameHeader; Name = 'обновление при изменении ширины окна' }
)) {
    if ($contract.Source -notmatch $contract.Text) {
        throw "Не найден обязательный контракт: $($contract.Name)."
    }
}

Write-Host 'Проверка настройки полного пути в заголовке окна прошла успешно.'
