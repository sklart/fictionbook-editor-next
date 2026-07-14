<#
.SYNOPSIS
Экспортирует подготовительные каталоги локализации в файлы для переводчиков.

.DESCRIPTION
Скрипт читает `localization/app-ui/catalog.json`,
`localization/plugin-ui/catalog.json` и `localization/installer-ui/catalog.json`,
проверяет общий набор языков и создаёт
временный каталог `out/localization/weblate-seed`. Для каждого языка формируется
отдельный JSON-файл со строками FBE/FBV и плагинов. Эти файлы не являются
runtime-ресурсами; они нужны как промежуточный формат для вычитки переводов и
будущего подключения Weblate.
#>
[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "out\localization\weblate-seed"
}

$appCatalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"
$pluginCatalogPath = Join-Path $repoRoot "localization\plugin-ui\catalog.json"
$runtimeContractPath = Join-Path $repoRoot "localization\runtime\contract.json"
$installerCatalogPath = Join-Path $repoRoot "localization\installer-ui\catalog.json"

$appCatalog = Get-Content -Raw -LiteralPath $appCatalogPath | ConvertFrom-Json -Depth 30
$pluginCatalog = Get-Content -Raw -LiteralPath $pluginCatalogPath | ConvertFrom-Json -Depth 30
$runtimeContract = Get-Content -Raw -LiteralPath $runtimeContractPath | ConvertFrom-Json -Depth 20
$installerCatalog = Get-Content -Raw -LiteralPath $installerCatalogPath | ConvertFrom-Json -Depth 30

$appLanguages = @($appCatalog.targetLanguages)
$pluginLanguages = @($pluginCatalog.targetLanguages)
foreach ($language in $appLanguages) {
    if ($language -notin $pluginLanguages) {
        throw "Язык $language есть в app-ui, но отсутствует в plugin-ui."
    }
}
foreach ($language in $pluginLanguages) {
    if ($language -notin $appLanguages) {
        throw "Язык $language есть в plugin-ui, но отсутствует в app-ui."
    }
}
foreach ($language in @($installerCatalog.targetLanguages)) {
    if ($language -notin $appLanguages) {
        throw "Язык $language есть в installer-ui, но отсутствует в app-ui."
    }
}
foreach ($language in $appLanguages) {
    if ($language -notin @($installerCatalog.targetLanguages)) {
        throw "Язык $language есть в app-ui, но отсутствует в installer-ui."
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

function Convert-CatalogStrings {
    param(
        [Parameter(Mandatory)]
        [object[]]$Properties,

        [Parameter(Mandatory)]
        [string]$Language,

        [Parameter(Mandatory)]
        [string]$Scope,

        [switch]$AllowEnglishFallback
    )

    $result = [ordered]@{}
    foreach ($entry in $Properties) {
        $translations = $entry.Value.translations
        $translation = $translations.PSObject.Properties[$Language]
        $needsTranslation = $false
        if (-not $translation) {
            if (-not $AllowEnglishFallback) {
                throw "В $Scope отсутствует перевод $($entry.Name) для $Language."
            }
            $translation = $translations.PSObject.Properties["en-US"]
            if (-not $translation) {
                throw "В $Scope отсутствует английский fallback для $($entry.Name)."
            }
            $needsTranslation = $true
        }

        $item = [ordered]@{
            scope = $Scope
            text = [string]$translation.Value
        }

        if ($entry.Value.PSObject.Properties["source"]) {
            $item.source = [string]$entry.Value.source
        }
        if ($entry.Value.PSObject.Properties["resourceId"]) {
            $item.resourceId = [string]$entry.Value.resourceId
        }
        if ($entry.Value.PSObject.Properties["component"]) {
            $item.component = [string]$entry.Value.component
        }
        if ($entry.Value.PSObject.Properties["comment"]) {
            $item.comment = [string]$entry.Value.comment
        }
        if ($entry.Value.PSObject.Properties["nsisName"]) {
            $item.nsisName = [string]$entry.Value.nsisName
        }
        if ($needsTranslation) {
            $item.needsTranslation = $true
        }

        $result[$entry.Name] = $item
    }
    return $result
}

$manifest = [ordered]@{
    formatVersion = 1
    generatedAt = (Get-Date).ToString("s")
    fallbackLanguage = [string]$runtimeContract.fallbackLanguage
    sourceCatalogs = @(
        "localization/app-ui/catalog.json",
        "localization/plugin-ui/catalog.json",
        "localization/installer-ui/catalog.json"
    )
    languages = $appLanguages
    stringCount = 0
    files = @()
    note = "Временный экспорт для переводчиков/Weblate. Не редактируется программой во время выполнения."
}

foreach ($language in $appLanguages) {
    $strings = [ordered]@{}
    $appStrings = Convert-CatalogStrings -Properties @($appCatalog.seedStrings.PSObject.Properties) -Language $language -Scope "app-ui"
    $pluginStrings = Convert-CatalogStrings -Properties @($pluginCatalog.strings.PSObject.Properties) -Language $language -Scope "plugin-ui"
    $installerStrings = Convert-CatalogStrings -Properties @($installerCatalog.strings.PSObject.Properties) -Language $language -Scope "installer-ui" -AllowEnglishFallback

    foreach ($item in $appStrings.GetEnumerator()) { $strings[$item.Key] = $item.Value }
    foreach ($item in $pluginStrings.GetEnumerator()) { $strings[$item.Key] = $item.Value }
    foreach ($item in $installerStrings.GetEnumerator()) { $strings[$item.Key] = $item.Value }

    $languageExport = [ordered]@{
        formatVersion = 1
        language = $language
        fallbackLanguage = [string]$runtimeContract.fallbackLanguage
        sourceCatalogs = $manifest.sourceCatalogs
        stringCount = $strings.Count
        strings = $strings
    }

    $json = $languageExport | ConvertTo-Json -Depth 20
    $targetPath = Join-Path $OutputDirectory "$language.json"
    [IO.File]::WriteAllText($targetPath, $json + "`n", [Text.UTF8Encoding]::new($false))

    if ($strings.Count -gt $manifest.stringCount) {
        $manifest.stringCount = $strings.Count
    }
    $manifest.files += [ordered]@{
        language = $language
        file = "$language.json"
        stringCount = $strings.Count
    }
}

$manifestPath = Join-Path $OutputDirectory "manifest.json"
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 10) + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "Экспорт локализационных seed-файлов подготовлен."
Write-Host "  Каталог: $OutputDirectory"
Write-Host "  Языков: $($appLanguages.Count)"
