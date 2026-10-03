<# Regression coverage for the localization debt ratchet. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$preflight = Join-Path $repoRoot 'tools\tests\test-localization-preflight.ps1'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("fbe-localization-preflight-" + [guid]::NewGuid().ToString('N'))
$locales = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
function Write-Json([string]$RelativePath, [object]$Value) {
    $path = Join-Path $tempRoot $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    [IO.File]::WriteAllText($path, ($Value | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))
}
function Invoke-Preflight([int]$ExpectedExitCode) {
    $previousPreference = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try { $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $preflight -RepositoryRoot $tempRoot -SkipStaticTests 2>&1 | Out-String; $exitCode = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exitCode -ne $ExpectedExitCode) { throw "Unexpected preflight exit code $exitCode (expected $ExpectedExitCode): $output" }
    return $output
}
try {
    $translations = [ordered]@{}; foreach ($locale in $locales) { $translations[$locale] = "Localized $locale" }
    $entry = [ordered]@{ translations = $translations }
    foreach ($catalog in @('localization\app-ui\catalog.json','localization\app-ui\fbe-idr-mainframe-menu.json','localization\app-ui\fbe-secondary-menus.json','localization\app-ui\fbe-small-dialogs.json','localization\plugin-ui\catalog.json')) { Write-Json $catalog ([ordered]@{ targetLanguages = $locales; strings = [ordered]@{ sample = $entry } }) }
    Write-Json 'localization\installer-ui\catalog.json' ([ordered]@{ targetLanguages = $locales; strings = [ordered]@{ sample = [ordered]@{ translations = [ordered]@{ 'en-US'='Installer'; 'ru-RU'='Установщик'; 'uk-UA'='Інсталятор' } } } })
    Write-Json 'localization\incomplete-translations-baseline.json' ([ordered]@{ formatVersion=1; catalogs=[ordered]@{} })
    $output = Invoke-Preflight 0
    if ($output -notmatch 'Known translation debt: 0 keys / 0 translations') { throw "Unexpected clean baseline report: $output" }

    $smallDialogPath = Join-Path $tempRoot 'localization\app-ui\fbe-small-dialogs.json'
    $catalog = Get-Content -Raw -LiteralPath $smallDialogPath | ConvertFrom-Json
    $catalog.strings.sample.translations.'de-DE' = ''
    Write-Json 'localization\app-ui\fbe-small-dialogs.json' $catalog
    $output = Invoke-Preflight 1
    if ($output -notmatch 'NEW missing translations:' -or $output -notmatch 'de-DE' -or $output -notmatch 'sample') { throw "New translation debt was not reported: $output" }

    $catalog.strings.sample.translations.PSObject.Properties.Remove('de-DE')
    Write-Json 'localization\app-ui\fbe-small-dialogs.json' $catalog
    Write-Json 'localization\incomplete-translations-baseline.json' ([ordered]@{ formatVersion=1; catalogs=[ordered]@{ 'localization\app-ui\fbe-small-dialogs.json'=[ordered]@{ sample=@('de-DE') } } })
    $output = Invoke-Preflight 0
    if ($output -notmatch 'Known translation debt: 1 keys / 1 translations') { throw "Known baseline debt was not accepted: $output" }

    $catalog.strings.sample.translations | Add-Member -Force -NotePropertyName 'de-DE' -NotePropertyValue 'Deutsch'
    Write-Json 'localization\app-ui\fbe-small-dialogs.json' $catalog
    $output = Invoke-Preflight 1
    if ($output -notmatch 'STALE baseline entries:' -or $output -notmatch 'translation is now present') { throw "Stale baseline entry was not reported: $output" }

    $appCatalogPath = Join-Path $tempRoot 'localization\app-ui\catalog.json'
    $appCatalog = Get-Content -Raw -LiteralPath $appCatalogPath | ConvertFrom-Json
    $appCatalog.strings.sample | Add-Member -NotePropertyName fallback -NotePropertyValue 'en-US'
    Write-Json 'localization\app-ui\catalog.json' $appCatalog
    $output = Invoke-Preflight 1
    if ($output -notmatch 'production fallback/needsTranslation is forbidden') { throw "Strict application catalog fallback was not rejected: $output" }
    $appCatalog.strings.sample.PSObject.Properties.Remove('fallback')
    $appCatalog.strings.sample.translations.'en-US' = 'Path: %s'
    $appCatalog.strings.sample.translations.'de-DE' = 'Pfad: %d'
    Write-Json 'localization\app-ui\catalog.json' $appCatalog
    $output = Invoke-Preflight 1
    if ($output -notmatch 'format placeholder set differs from en-US' -or $output -notmatch 'de-DE') { throw "Placeholder mismatch was not reported: $output" }
}
finally { if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force } }
Write-Host 'Localization preflight ratchet regression passed.'