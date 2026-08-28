<#
.SYNOPSIS
Checks the source-level contract of the contextual FBE status bar.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$main = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\mainfrm.cpp")
$unicode = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\StatusBarUnicode.h")
$header = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\mainfrm.h")
$resources = Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\resource.h")
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root "localization\app-ui\catalog.json") | ConvertFrom-Json

foreach ($pane in @("ID_PANE_POSITION", "ID_PANE_SELECTION", "ID_PANE_CHAR", "ID_PANE_ENCODING", "ID_PANE_VALIDATION", "ID_PANE_INS")) {
    if ($resources -notmatch "(?m)^#define\s+$pane\b") { throw "Missing status pane ID: $pane" }
}
if ($main -match "SetPaneWidth\s*\(\s*399\b" -or $main -match "\b399\s*,\s*ID_PANE_INS") {
    throw "The historical magic status pane ID 399 is still used."
}
foreach ($api in @("SCI_COUNTCHARACTERS", "SCI_POSITIONBEFORE", "SCI_LINEFROMPOSITION", "SCI_POSITIONFROMLINE", "SCI_GETLINECOUNT")) {
    if ($main -notmatch $api) { throw "Unicode-aware Scintilla API is missing: $api" }
}
if ($unicode -notmatch "FirstCodePoint" -or $unicode -notmatch "0x10000") {
    throw "Supplementary UTF-16 code point handling is missing."
}
if ($main -notmatch "&#%u;") { throw "Decimal XML character reference is missing." }
foreach ($contract in @("CurrentOverwriteMode", "SetValidationStatus", "ResetValidationStatus", "ResetStatusForDocument", "RefreshStatusMainPane")) {
    if ($main -notmatch $contract -or $header -notmatch $contract) { throw "Missing status lifecycle helper: $contract" }
}
if ($main -notmatch "caret > 0") { throw "SOURCE caret-at-zero guard is missing." }
if ($main -notmatch "static_cast<LONG>\(::GetTickCount\(\) - m_status_transient_expiration\)") { throw "Transient timeout is not wrap-safe." }
if ($main -notmatch "CurrentOverwriteMode\(\) \? strOVR : strINS") { throw "INS/OVR does not use the centralized view-aware mode." }
if ($main -notmatch "UpdateStatusBarLayout" -or $header -notmatch "UpdateStatusBarLayout") {
    throw "Status bar must use the common dynamic layout path."
}
foreach ($key in @("fbe.status.position", "fbe.status.selection")) {
    $entry = $catalog.seedStrings.$key
    if ($null -eq $entry) { throw "Missing localization key: $key" }
    foreach ($locale in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG")) {
        if ([string]::IsNullOrWhiteSpace([string]$entry.translations.$locale)) { throw "Missing $locale translation for $key" }
		$placeholders = [regex]::Matches([string]$entry.translations.$locale, '(?<!%)%[di]').Count
		$expected = 3
		if ($placeholders -ne $expected) { throw "$locale translation for $key has $placeholders integer placeholders; expected $expected." }
    }
}
foreach ($key in @("fbe.status_pane.position", "fbe.status_pane.selection", "fbe.status_pane.character", "fbe.status_pane.encoding", "fbe.status_pane.validation", "fbe.status_pane.insert_mode")) {
	$entry = $catalog.seedStrings.$key
	if ($null -eq $entry) { throw "Missing localization key: $key" }
	if (@($entry.translations.PSObject.Properties).Count -ne 12) { throw "$key must have translations for all 12 locales." }
}
foreach ($contract in @("StatusPaneAt", "ToggleStatusPaneVisibility", "OnStatusBarDoubleClick")) {
	if ($main -notmatch $contract -or $header -notmatch $contract) { throw "Missing status bar interaction: $contract" }
}
if ($main -notmatch "SourceBreadcrumb") { throw "Missing SOURCE breadcrumb integration." }
Write-Host "Contextual status bar contract passed."
