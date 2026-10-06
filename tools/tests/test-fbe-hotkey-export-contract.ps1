<# Contract for the Settings hotkey TXT/HTML export surface. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([string]$text, [string]$pattern, [string]$message) { if($text -notmatch $pattern) { throw $message } }
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\hotkeys\HotkeyExport.cpp')
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
foreach($required in 'ExportData BuildExportData','BuildTextExport\(const ExportData','BuildHtmlExport\(const ExportData','EscapeHtml','WriteUtf8ExportFile','U::AccelToString\(hotkey\.m_accel\)') { Require $source $required "Missing shared hotkey export contract: $required" }
Require $source 'hotkey\.m_accel\.key == 0\) continue' 'Unassigned shortcuts must not be exported.'
Require $source 'GetHotkeyGroupDisplayName\(group\)' 'Export must use localized group names.'
Require $source 'GetHotkeyDisplayName\(hotkey\)' 'Export must use localized command names.'
Require $source 'BuildHtmlExport\(BuildExportData\(groups\)' 'TXT and HTML must share one export-data builder.'
Require $source 'L"&amp;"' 'HTML renderer must escape ampersands.'
Require $source 'L"&lt;"' 'HTML renderer must escape less-than signs.'
Require $source 'L"&gt;"' 'HTML renderer must escape greater-than signs.'
Require $source 'L"&quot;"' 'HTML renderer must escape quotes.'
Require $source '<!doctype html>' 'HTML renderer must emit HTML5 doctype.'
Require $source '<meta charset=' 'HTML renderer must declare UTF-8.'
Require $source '<kbd>' 'HTML renderer must render shortcuts semantically.'
Require $utils 'KeycodeToDisplayString' 'Shared accelerator formatter must expose display-key localization.'
Require $dialogSource 'BuildExportData\(_Settings\.m_hotkey_groups\)' 'Dialog must build shared export data once.'
Require $dialogSource 'BuildHtmlExport\(data, _Settings\.GetInterfaceLocaleName\(\)\)' 'HTML export must use current locale.'
Require $dialogSource 'BuildTextExport\(data\)' 'TXT export must use same data.'
Require $dialogSource 'WriteUtf8ExportFile' 'Both formats must use shared UTF-8 writer.'
Require $dialogSource 'FBE-Next-Hotkeys\.html' 'HTML must be the default proposed file name.'
Require $dialogSource 'nFilterIndex != 2' 'HTML filter must be first and TXT filter second.'
Require $dialogSource 'fbe\.hotkey\.export\.error' 'Export error must use runtime localization.'
Require $locale 'fbe\.dialog\.idd_hotkeys\.export' 'Export button localization is missing.'
foreach($key in 'fbe.hotkey.export.title','fbe.hotkey.export.column.command','fbe.hotkey.export.column.shortcut','fbe.hotkey.export.filter.html','fbe.hotkey.export.filter.text','fbe.hotkey.export.error','fbe.hotkey.key.space','fbe.hotkey.key.backspace','fbe.hotkey.key.delete','fbe.hotkey.key.insert') {
    $entry = $catalog.seedStrings.PSObject.Properties[$key].Value
    if($null -eq $entry) { throw "Missing hotkey runtime localization key: $key" }
    foreach($language in $catalog.targetLanguages) { if([string]::IsNullOrWhiteSpace([string]$entry.translations.PSObject.Properties[$language].Value)) { throw "Missing $language localization for $key." } }
}
if($catalog.seedStrings.'fbe.hotkey.export.title'.translations.'ru-RU' -ne 'Горячие клавиши') { throw 'Russian hotkey export title must be fully localized.' }
if($catalog.seedStrings.'fbe.hotkey.export.column.command'.translations.'ru-RU' -ne 'Команда' -or $catalog.seedStrings.'fbe.hotkey.export.column.shortcut'.translations.'ru-RU' -ne 'Горячая клавиша') { throw 'Russian HTML table headers must be localized.' }
if($catalog.seedStrings.'fbe.hotkey.key.space'.translations.'ru-RU' -ne 'Пробел') { throw 'Russian Space key name must be localized through the shared formatter.' }
Write-Host 'Hotkey TXT/HTML export contract passed.'