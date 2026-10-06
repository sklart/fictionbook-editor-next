<# Contract for the Settings hotkey text export surface. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([string]$text, [string]$pattern, [string]$message) { if($text -notmatch $pattern) { throw $message } }
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\hotkeys\HotkeyTextExport.cpp')
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\ui\SettingsHotkeysDlg.cpp')
$utils = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\utils\Utils.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\ui\SettingsHotkeysDlg.h')
$resource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
$ids = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
$locale = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json') | ConvertFrom-Json
Require $ids 'IDC_BUTTON_HOTKEY_EXPORT' 'Missing hotkey Export control id.'
if(([regex]::Matches($ids, '^#define\s+\w+\s+1669\b', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count -ne 1) { throw 'Hotkey Export control id must be unique.' }
Require $resource 'PUSHBUTTON\s+"Export\.\.\.",IDC_BUTTON_HOTKEY_EXPORT' 'Missing Export button in the hotkeys page.'
Require $header 'OnBnClickedButtonHotkeyExport' 'Export button is not routed.'
Require $source 'BuildTextExport' 'Missing human-readable hotkey export formatter.'
Require $dialogSource 'FbeSettings::Hotkeys::BuildTextExport\(_Settings\.m_hotkey_groups\)' 'Export button must use the production text formatter.'
Require $source 'hotkey\.m_accel\.key == 0\) continue' 'Unassigned shortcuts must not be exported.'
Require $source 'GetHotkeyGroupDisplayName\(group\)' 'Export must use localized group names.'
Require $source 'GetHotkeyDisplayName\(hotkey\)' 'Export must use localized command names.'
Require $source 'U::AccelToString\(hotkey\.m_accel\)' 'Export must use the shared UI accelerator formatter, not raw ACCEL fields.'
Require $source 'fbe\.hotkey\.export\.title' 'Export title must come from runtime localization.'
Require $utils 'KeycodeToDisplayString' 'Shared accelerator formatter must expose display-key localization.'
Require $dialogSource 'WideCharToMultiByte\(CP_UTF8' 'Export must be UTF-8.'
Require $dialogSource 'FBE-Next-Hotkeys\.txt' 'Export must propose the documented file name.'
Require $locale 'fbe\.dialog\.idd_hotkeys\.export' 'Export button localization is missing.'
foreach($key in 'fbe.hotkey.export.title','fbe.hotkey.key.space','fbe.hotkey.key.backspace','fbe.hotkey.key.delete','fbe.hotkey.key.insert') {
    $entry = $catalog.seedStrings.PSObject.Properties[$key].Value
    if($null -eq $entry) { throw "Missing hotkey runtime localization key: $key" }
    foreach($language in $catalog.targetLanguages) { if([string]::IsNullOrWhiteSpace([string]$entry.translations.PSObject.Properties[$language].Value)) { throw "Missing $language localization for $key." } }
}
if($catalog.seedStrings.'fbe.hotkey.export.title'.translations.'ru-RU' -ne 'Горячие клавиши') { throw 'Russian hotkey export title must be fully localized.' }
if($catalog.seedStrings.'fbe.hotkey.key.space'.translations.'ru-RU' -ne 'Пробел') { throw 'Russian Space key name must be localized through the shared formatter.' }
Write-Host 'Hotkey export contract passed.'
