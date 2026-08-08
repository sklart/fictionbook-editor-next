$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$data = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-idr-mainframe-menu.json') | ConvertFrom-Json
$languages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
$keys = @('fbe.menu.idr_mainframe.tools.open_diagnostic_log','fbe.menu.idr_mainframe.tools.open_diagnostic_folder','fbe.menu.idr_mainframe.tools.copy_diagnostic_log_path','fbe.menu.idr_mainframe.tools.clear_diagnostic_logs','fbe.menu.idr_mainframe.tools.create_diagnostic_package','fbe.trace.clear_confirmation','fbe.trace.clear_completed','fbe.trace.clear_failed','fbe.trace.clear_completed_details','fbe.trace.clear_partial','fbe.trace.clear_empty','fbe.trace.clear_delete_failed','fbe.trace.open_folder_failed','fbe.trace.copy_path_failed','fbe.trace.package_created','fbe.trace.package_failed')
foreach($key in $keys) {
    $entryProperty = $data.strings.PSObject.Properties | Where-Object Name -eq $key
    if($null -eq $entryProperty) { throw "Missing diagnostic localization key: $key" }
    $entry = $entryProperty.Value
    $translations = $entry.translations
    if($null -eq $translations) { throw "Missing translations object for $key" }
    foreach($language in $languages) {
        $translationProperty = $translations.PSObject.Properties | Where-Object Name -eq $language
        if($null -eq $translationProperty -or [string]::IsNullOrWhiteSpace([string]$translationProperty.Value)) {
            throw "Missing ${language} translation for $key"
        }
    }
}
Write-Host 'Diagnostic localization contract passed.'
