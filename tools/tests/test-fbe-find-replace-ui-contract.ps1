<#
.SYNOPSIS
Guards the common Find/Replace controls, their localized labels, and Results
Pane painting contracts without materializing virtual ListView rows.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$rc = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SearchReplace.h')
$pane = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json

function Require([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}
function DialogBlock([string]$id) {
    $match = [regex]::Match($rc, "(?s)$id DIALOGEX.*?^END", [Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) { throw "Dialog $id was not found." }
    return $match.Value
}
function RequireLocalized([string]$key) {
    $entry = $catalog.strings.$key
    if ($null -eq $entry -or [string]::IsNullOrWhiteSpace($entry.translations.'en-US') -or [string]::IsNullOrWhiteSpace($entry.translations.'ru-RU')) {
        throw "Missing English or Russian runtime localization for $key."
    }
}

$find = DialogBlock 'IDD_FIND'
$replace = DialogBlock 'IDD_REPLACE'
foreach ($control in @('IDC_WHOLE', 'IDC_MATCHCASE', 'IDC_REGEXP', 'IDC_FIND_SCOPE_LABEL', 'IDC_FIND_SCOPE', 'IDC_FIND_UNICODE_PROPERTIES', 'IDC_UP', 'IDC_DOWN', 'IDC_FIND_FROM_START')) {
    Require $find $control "Find control $control"
    Require $replace $control "Replace control $control"
}
Require $find 'IDC_FIND_SCOPE[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Find UCP immediately follows Scope'
Require $replace 'IDC_FIND_SCOPE[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Replace UCP immediately follows Scope'
Require $find 'IDC_FIND_LABEL_TEXT,7,9,45,8[\s\S]*?IDC_TEXT,53,7,179,62' 'Find label and input use the shared horizontal grid'
Require $replace 'IDC_REPLACE_LABEL_TEXT,7,9,45,8[\s\S]*?IDC_TEXT,53,7,179,62' 'Replace label and input use the shared horizontal grid'
foreach ($dialogBlock in @($find, $replace)) {
    Require $dialogBlock 'IDC_WHOLE,"Button",BS_AUTOCHECKBOX \| WS_TABSTOP,7,' 'common options start at x=7'
    Require $dialogBlock 'IDC_FIND_SCOPE_LABEL,105,' 'Scope starts at x=105'
    Require $dialogBlock 'DIRECTION_GROUP,172,' 'Direction starts at x=172'
    Require $dialogBlock 'ID_FIND_NEXT,238,7,50,14' 'Find Next uses the shared action column'
    Require $dialogBlock 'IDCANCEL,238,61,50,14' 'Cancel uses the shared bottom action slot'
}
Require $find 'IDC_FIND_ALL' 'Find All action'
Require $replace 'IDC_REPLACE_ONE[\s\S]*?IDC_REPLACE_ALL' 'Replace-specific actions'
if ($replace -match 'IDC_FIND_ALL') { throw 'Replace must not add a duplicate Find All action.' }
Require $dialog 'GetDlgItem\(IDC_FIND_SCOPE\) != NULL[\s\S]*?PopulateFindScopes' 'common scope initialization'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnFindFromStart[\s\S]*?DoSearchFromScopeStart' 'Replace From start action'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnScopeChanged[\s\S]*?ResetSearchScope' 'Replace preserves a stable scope until the user changes it'
Require $dialog 'EnableWindow\(unicode, ::IsDlgButtonChecked' 'UCP remains RegExp-gated in both dialogs'
Require $pane 'm_list\.GetItemState\(item, LVIS_SELECTED\) & LVIS_SELECTED' 'authoritative ListView selection painting'
Require $pane 'GetSysColorBrush\(selected \? COLOR_HIGHLIGHT : COLOR_WINDOW\)' 'complete selected and unselected cell repaint'
Require $pane 'L" \\x2014 \\x00AB" \+ query \+ L"\\x00BB \\x2014 "' 'Unicode-safe Results Pane header punctuation'
if ($pane -match 'title \+= L" —') { throw 'Results Pane header must not depend on a source-code-page em dash literal.' }

$caption = $catalog.strings.'fbe.dialog.idd_find_results.caption'.translations
$count = $catalog.strings.'fbe.dialog.idd_find_results.count'.translations
if ($caption.'ru-RU' -ne 'Результаты поиска' -or $count.'ru-RU' -ne 'Найдено: %Iu') { throw 'Russian Results Pane header localization is not canonical.' }
if ($caption.'en-US' -ne 'Find results' -or $count.'en-US' -ne 'Found: %Iu') { throw 'English Results Pane header localization is not canonical.' }

foreach ($key in @(
    'fbe.dialog.idd_find.unicode_properties', 'fbe.dialog.idd_find.scope', 'fbe.dialog.idd_find.from_start',
    'fbe.replace.preview.completed', 'fbe.tooltip.find.unicode_properties')) {
    RequireLocalized $key
}
foreach ($key in @('fbe.dialog.idd_replace.unicode_properties', 'fbe.dialog.idd_replace.scope', 'fbe.dialog.idd_replace.from_start')) {
    if ($null -ne $catalog.strings.$key) { throw "Duplicate Replace localization key $key must not exist." }
}
Require $dialog 'SyncSearchOptionsToOpenDialogs\(this\)' 'immediate Find/Replace common-option synchronization'
Require $dialog 'SyncSearchOptionsFromView' 'peer dialog control synchronization'
Require $dialog 'm_tooltips\.Add\(GetDlgItem\(IDC_FIND_UNICODE_PROPERTIES\), L"fbe\.tooltip\.find\.unicode_properties"[\s\S]*?m_tooltips\.AddDisabledControlArea\(GetDlgItem\(IDC_FIND_UNICODE_PROPERTIES\), L"fbe\.tooltip\.find\.unicode_properties"' 'shared enabled and disabled UCP tooltip delivery'
Require $dialog 'Use Unicode properties for \\\\w, \\\\d, \\\\s and word boundaries \\\\b/\\\\B \(for example with Cyrillic text\)\. Available only when Regular expression is enabled\.' 'informative portable UCP tooltip fallback'
Require $pane 'fbe\.dialog\.idd_find_results\.count", L"Found: %Iu"' 'neutral portable Results Pane count fallback'
Require $view 'fbe\.replace\.preview\.message", L"Number of replacements: %Iu\. Continue\?"' 'neutral portable Replace All confirmation fallback'
if ($dialog -match 'idd_replace\.(unicode_properties|scope|from_start)') { throw 'Replace must use shared Find localization keys for common controls.' }
if ($catalog.strings.'fbe.replace.preview.message'.translations.'ru-RU' -ne 'Количество замен: %Iu. Продолжить?') { throw 'Russian Replace All confirmation is not canonical.' }
if ($catalog.strings.'fbe.replace.preview.message'.translations.'en-US' -ne 'Number of replacements: %Iu. Continue?') { throw 'English Replace All confirmation is not canonical.' }
foreach ($language in @('en-US', 'ru-RU', 'uk-UA', 'de-DE', 'fr-FR', 'es-ES', 'it-IT', 'pl-PL', 'pt-PT', 'nl-NL', 'cs-CZ', 'bg-BG')) {
    if ([string]::IsNullOrWhiteSpace($count.$language) -or [string]::IsNullOrWhiteSpace($catalog.strings.'fbe.replace.preview.message'.translations.$language)) {
        throw "Neutral search count or Replace All confirmation is missing for $language."
    }
}
if ($null -ne $catalog.strings.'fbe.replace.preview.ready') { throw 'Obsolete second-click Replace All prompt must not remain localized.' }
if ($catalog.strings.'fbe.tooltip.find.unicode_properties'.translations.'ru-RU' -ne 'Использовать Unicode-свойства для \w, \d, \s и границ слов \b/\B (например, для кириллицы). Доступно только при включённом «Регулярное выражение».') { throw 'Russian UCP tooltip is not canonical.' }
if ($catalog.strings.'fbe.tooltip.find.unicode_properties'.translations.'en-US' -ne 'Use Unicode properties for \w, \d, \s and word boundaries \b/\B (for example with Cyrillic text). Available only when Regular expression is enabled.') { throw 'English UCP tooltip is not canonical.' }

Write-Host 'Find/Replace common UI and Results Pane contract passed.'
