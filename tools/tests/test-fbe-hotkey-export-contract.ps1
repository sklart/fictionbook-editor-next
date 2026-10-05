<# Contract for the Settings hotkey text export surface. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([string]$text, [string]$pattern, [string]$message) { if($text -notmatch $pattern) { throw $message } }
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\ui\SettingsHotkeysDlg.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\ui\SettingsHotkeysDlg.h')
$resource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
$ids = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
$locale = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json')
Require $ids 'IDC_BUTTON_HOTKEY_EXPORT' 'Missing hotkey Export control id.'
if(([regex]::Matches($ids, '^#define\s+\w+\s+1669\b', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count -ne 1) {
    throw 'Hotkey Export control id must be unique.'
}
Require $resource 'PUSHBUTTON\s+"Export\.\.\.",IDC_BUTTON_HOTKEY_EXPORT' 'Missing Export button in the hotkeys page.'
Require $header 'OnBnClickedButtonHotkeyExport' 'Export button is not routed.'
Require $source 'BuildHotkeysExportText' 'Missing human-readable hotkey export formatter.'
Require $source 'hotkey\.m_accel\.key == 0\) continue' 'Unassigned shortcuts must not be exported.'
Require $source 'GetHotkeyGroupDisplayName\(group\)' 'Export must use localized group names.'
Require $source 'GetHotkeyDisplayName\(hotkey\)' 'Export must use localized command names.'
Require $source 'U::AccelToString\(hotkey\.m_accel\)' 'Export must use formatted accelerators, not raw ACCEL fields.'
Require $source 'WideCharToMultiByte\(CP_UTF8' 'Export must be UTF-8.'
Require $source 'FBE-Next-Hotkeys\.txt' 'Export must propose the documented file name.'
Require $locale 'fbe\.dialog\.idd_hotkeys\.export' 'Export button localization is missing.'
Write-Host 'Hotkey export contract passed.'
