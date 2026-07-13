<#
.SYNOPSIS
Проверяет инвентарь будущих языковых пакетов установщика.

.DESCRIPTION
Скрипт валидирует `localization/language-packs.json`: языки должны совпадать с
app/plugin каталогами, fallback-язык должен существовать, а уже существующие
ресурсы проекта должны быть описаны в инвентаре. Файлы из `futureTemplates` не
требуются физически, потому что это задел под будущие локализованные шаблоны.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$packPath = Join-Path $repoRoot "localization\language-packs.json"
$appCatalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"
$pluginCatalogPath = Join-Path $repoRoot "localization\plugin-ui\catalog.json"

$packs = Get-Content -Raw -LiteralPath $packPath | ConvertFrom-Json -Depth 30
$app = Get-Content -Raw -LiteralPath $appCatalogPath | ConvertFrom-Json -Depth 30
$plugin = Get-Content -Raw -LiteralPath $pluginCatalogPath | ConvertFrom-Json -Depth 30

$languages = @($packs.languages)
if ($languages.Count -eq 0) {
    throw "В language-packs.json нет языков."
}

$languageCodes = @($languages | ForEach-Object { $_.language })
$duplicates = $languageCodes | Group-Object | Where-Object { $_.Count -gt 1 }
if ($duplicates) {
    throw "В language-packs.json есть дубли языков: $($duplicates.Name -join ', ')"
}

if ($packs.fallbackLanguage -notin $languageCodes) {
    throw "Fallback-язык отсутствует в списке языков: $($packs.fallbackLanguage)"
}

if (-not $packs.languageNeutralAssets -or -not $packs.languageNeutralAssets.shellMuiHost) {
    throw "В language-packs.json отсутствует languageNeutralAssets.shellMuiHost для MUI-host shell-команд."
}
if (@($packs.languageNeutralAssets.shellMuiHost) -notcontains "FBVVerbResources.dll") {
    throw "MUI-host shell-команд должен планироваться рядом с приложением как FBVVerbResources.dll."
}

foreach ($language in @($app.targetLanguages)) {
    if ($language -notin $languageCodes) {
        throw "Язык $language есть в app-ui catalog, но отсутствует в language-packs.json."
    }
}
foreach ($language in @($plugin.targetLanguages)) {
    if ($language -notin $languageCodes) {
        throw "Язык $language есть в plugin-ui catalog, но отсутствует в language-packs.json."
    }
}

foreach ($entry in $languages) {
    if ([string]::IsNullOrWhiteSpace([string]$entry.displayName)) {
        throw "У языка $($entry.language) отсутствует displayName."
    }

    if (-not $entry.assets) {
        throw "У языка $($entry.language) отсутствует assets."
    }

    if (-not $entry.assets.fbvMui) {
        throw "У языка $($entry.language) отсутствует fbvMui-ресурс для shell-команды Validate."
    }
    $expectedMui = "$($entry.language)/FBVVerbResources.dll.mui"
    if (@($entry.assets.fbvMui) -notcontains $expectedMui) {
        throw "У языка $($entry.language) MUI-ресурс должен лежать в $expectedMui."
    }
}

$knownRepoAssets = @(
    "res_rus.dll",
    "res_ukr.dll",
    "gpl-3.0.txt",
    "gpl-3.0.ru.txt",
    "gpl-3.0.ua.txt",
    "genres.rus.txt",
    "genres.rus.txt_L",
    "genres.ukr.txt",
    "rus.xsl",
    "ukr.xsl"
)

$declaredAssets = New-Object System.Collections.Generic.HashSet[string]
foreach ($entry in $languages) {
    foreach ($assetGroup in $entry.assets.PSObject.Properties) {
        if ($assetGroup.Name -eq "futureTemplates") {
            continue
        }
        foreach ($asset in @($assetGroup.Value)) {
            [void]$declaredAssets.Add([string]$asset)
        }
    }
}

foreach ($asset in $knownRepoAssets) {
    if (-not $declaredAssets.Contains($asset)) {
        throw "Инвентарь языковых пакетов не описывает известный языковой ресурс: $asset"
    }
}

Write-Host "Инвентарь языковых пакетов установщика прошёл проверку."
Write-Host "  Файл: $packPath"
Write-Host "  Языков: $($languageCodes.Count)"
Write-Host "  Fallback: $($packs.fallbackLanguage)"
