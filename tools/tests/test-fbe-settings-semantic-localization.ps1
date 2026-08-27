[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$catalog = Get-Content -Raw -LiteralPath (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
function Assert-Translation($key, $resource, $id, $en, $ru) {
    $entry = $catalog.strings.$key
    if (-not $entry -or $entry.resource -ne $resource -or $entry.targetId -ne $id -or $entry.translations.'en-US' -ne $en -or $entry.translations.'ru-RU' -ne $ru) { throw "Semantic localization mismatch: $key" }
}
Assert-Translation 'fbe.dialog.idd_setting_other.keep_manual' 'IDD_SETTINGS_GENERAL' 'IDC_KEEP' 'Keep original encoding' 'Сохранять исходную кодировку'
Assert-Translation 'fbe.dialog.idd_setting_other.restore_position' 'IDD_SETTINGS_GENERAL' 'IDC_RESTORE_POS' 'Restore document position' 'Восстанавливать позицию в документе'
Assert-Translation 'fbe.dialog.idd_setting_other.clear_images' 'IDD_SETTINGS_IMAGES' 'IDC_OPTIONS_CLEARIMGS' 'Insert empty images' 'Вставлять пустые изображения'
Assert-Translation 'fbe.dialog.idd_setting_other.image_settings' 'IDD_SETTINGS_IMAGES' 'IDC_SETTINGS_OTHER_PASTE' 'Paste images as' 'Вставлять изображения как'
Assert-Translation 'fbe.dialog.idd_setting_other.nbsp_char' 'IDD_SETTINGS_EDITOR' 'IDC_SETTINGS_OTHER_NBSP_LABEL' 'Display character:' 'Отображаемый символ:'
Write-Host 'Settings semantic localization passed.'
