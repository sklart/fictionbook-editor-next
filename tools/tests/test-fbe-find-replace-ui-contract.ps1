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
Require $find 'IDC_FIND_ALL' 'Find All action'
Require $replace 'IDC_REPLACE_ONE[\s\S]*?IDC_REPLACE_ALL' 'Replace-specific actions'
if ($replace -match 'IDC_FIND_ALL') { throw 'Replace must not add a duplicate Find All action.' }
Require $dialog 'GetDlgItem\(IDC_FIND_SCOPE\) != NULL[\s\S]*?PopulateFindScopes' 'common scope initialization'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnFindFromStart[\s\S]*?DoSearchFromScopeStart' 'Replace From start action'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnScopeChanged[\s\S]*?ResetSearchScope' 'Replace preserves a stable scope until the user changes it'
Require $dialog 'EnableWindow\(unicode, ::IsDlgButtonChecked' 'UCP remains RegExp-gated in both dialogs'
Require $pane 'nmcd\.uItemState & CDIS_SELECTED' 'notification-owned selection painting'
Require $pane 'GetSysColorBrush\(selected \? COLOR_HIGHLIGHT : COLOR_WINDOW\)' 'complete selected and unselected cell repaint'
Require $pane 'L" \\x2014 \\x00AB" \+ query \+ L"\\x00BB \\x2014 "' 'Unicode-safe Results Pane header punctuation'
if ($pane -match 'title \+= L" —') { throw 'Results Pane header must not depend on a source-code-page em dash literal.' }

$caption = $catalog.strings.'fbe.dialog.idd_find_results.caption'.translations
$count = $catalog.strings.'fbe.dialog.idd_find_results.count'.translations
if ($caption.'ru-RU' -ne 'Результаты поиска' -or $count.'ru-RU' -ne '%Iu результатов') { throw 'Russian Results Pane header localization is not canonical.' }
if ($caption.'en-US' -ne 'Find results' -or $count.'en-US' -ne '%Iu results') { throw 'English Results Pane header localization is not canonical.' }

foreach ($key in @(
    'fbe.dialog.idd_find.unicode_properties', 'fbe.dialog.idd_find.scope', 'fbe.dialog.idd_find.from_start',
    'fbe.dialog.idd_replace.unicode_properties', 'fbe.dialog.idd_replace.scope', 'fbe.dialog.idd_replace.from_start')) {
    RequireLocalized $key
}

Write-Host 'Find/Replace common UI and Results Pane contract passed.'
