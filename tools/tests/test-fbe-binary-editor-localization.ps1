$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json') | ConvertFrom-Json
$keys = @('fbe.binary.id.empty', 'fbe.binary.id.duplicate', 'fbe.binary.delete.referenced')
$languages = @('en-US', 'ru-RU', 'uk-UA', 'de-DE', 'fr-FR', 'es-ES', 'it-IT', 'pl-PL', 'pt-PT', 'nl-NL', 'cs-CZ', 'bg-BG')
foreach($key in $keys) {
    if($catalog.seedStrings.PSObject.Properties[$key] -eq $null) { throw "Localization key missing: $key" }
    foreach($language in $languages) {
        if([string]::IsNullOrWhiteSpace([string]$catalog.seedStrings.$key.translations.$language)) { throw "Missing $language translation for $key" }
    }
    if($script -notmatch [regex]::Escape('LocalizedBinaryMessage("'+$key+'")')) { throw "main.js does not request localized key: $key" }
}
foreach($hardcoded in @('Binary ID must not be empty.', 'A binary with this ID already exists.', 'This binary is still used by the book or its cover and cannot be deleted.')) {
    if($script.Contains($hardcoded)) { throw "Hardcoded binary message remains in main.js: $hardcoded" }
}
Write-Host 'Binary editor localization contracts passed.'
