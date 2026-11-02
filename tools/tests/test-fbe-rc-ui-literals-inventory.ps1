<# Checks the current JSON replacement for the retired localized RC inventory. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

# Localized RC files were removed when FBE switched to runtime JSON. Their
# return would bypass the catalog checks below and recreate the old problem.
foreach ($locale in @('res_rus', 'res_ukr')) {
    $legacy = Join-Path $repoRoot "src\locales\$locale\FBE.rc"
    if (Test-Path -LiteralPath $legacy) { throw "Obsolete localized FBE.rc returned: $legacy" }
}
$baseRc = Get-Content -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc') -Raw
foreach ($resource in @('IDR_MAINFRAME MENU', 'IDD_FIND DIALOGEX', 'IDD_REPLACE DIALOGEX', 'IDD_SETTINGS_GENERAL DIALOGEX')) {
    if ($baseRc -notmatch [regex]::Escape($resource)) { throw "Missing base resource: $resource" }
}
$menu = Get-Content -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-idr-mainframe-menu.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$dialogs = Get-Content -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($menu.resource -ne 'IDR_MAINFRAME' -or $menu.resourceType -ne 'MENU') { throw 'Main menu catalog identity is invalid.' }
foreach ($resource in @('IDD_FIND', 'IDD_REPLACE', 'IDD_SETTINGS_GENERAL')) {
    if ($resource -notin @($dialogs.resources)) { throw "Dialog missing from JSON catalog: $resource" }
}
foreach ($catalog in @($menu, $dialogs)) {
    $entries = @($catalog.strings.PSObject.Properties)
    if ($entries.Count -eq 0) { throw 'Localization catalog is empty.' }
    foreach ($entry in $entries) {
        foreach ($locale in @('ru-RU', 'uk-UA')) {
            $translation = $entry.Value.translations.PSObject.Properties[$locale]
            if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) { throw "Missing $locale translation: $($entry.Name)" }
        }
    }
}
Write-Host 'FBE menu/dialog JSON inventory: PASS (legacy localized RC absent; ru-RU and uk-UA populated).'
