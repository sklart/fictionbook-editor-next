<#
Проверяет Weblate-friendly каталог пользовательских строк плагинов.
Скрипт валидирует JSON, обязательные языки и наличие непустого перевода для
каждого ключа, чтобы каталог можно было безопасно использовать как основу для
последующей генерации Win32 resource-фрагментов.
#>
[CmdletBinding()]
param(
    [string]$CatalogPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $CatalogPath) {
    $CatalogPath = Join-Path $repoRoot "localization\plugin-ui\catalog.json"
}

if (-not (Test-Path -LiteralPath $CatalogPath)) {
    throw "Каталог локализации плагинов не найден: $CatalogPath"
}

$catalog = Get-Content -Raw -LiteralPath $CatalogPath | ConvertFrom-Json
if ($catalog.formatVersion -ne 1) {
    throw "Неожиданная версия формата каталога: $($catalog.formatVersion)"
}

$requiredLanguages = @(
    "en-US",
    "ru-RU",
    "uk-UA",
    "de-DE",
    "fr-FR",
    "es-ES",
    "it-IT",
    "pl-PL",
    "pt-PT",
    "nl-NL",
    "cs-CZ",
    "bg-BG"
)

$declaredLanguages = @($catalog.targetLanguages)
foreach ($language in $requiredLanguages) {
    if ($declaredLanguages -notcontains $language) {
        throw "В targetLanguages отсутствует обязательный язык: $language"
    }
}

if ((@($declaredLanguages | Sort-Object -Unique)).Count -ne $declaredLanguages.Count) {
    throw "В targetLanguages есть дублирующиеся языки."
}

$stringProperties = @($catalog.strings.PSObject.Properties)
if ($stringProperties.Count -eq 0) {
    throw "Каталог не содержит ни одной строки."
}

$seenKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($property in $stringProperties) {
    $key = [string]$property.Name
    if (-not $seenKeys.Add($key)) {
        throw "Дублирующийся ключ локализации: $key"
    }

    if ($key -notmatch '^[a-z0-9_]+(\.[a-z0-9_]+)+$') {
        throw "Ключ локализации имеет нестабильный формат: $key"
    }

    $entry = $property.Value
    if ([string]::IsNullOrWhiteSpace([string]$entry.source)) {
        throw "У ключа $key отсутствует source-строка."
    }

    if (-not $entry.translations) {
        throw "У ключа $key отсутствует блок translations."
    }

    foreach ($language in $requiredLanguages) {
        $translationProperty = $entry.translations.PSObject.Properties[$language]
        if (-not $translationProperty) {
            throw "У ключа $key отсутствует перевод для $language."
        }
        if ([string]::IsNullOrWhiteSpace([string]$translationProperty.Value)) {
            throw "У ключа $key пустой перевод для $language."
        }
    }
}

Write-Host "Каталог локализации плагинов прошёл проверку."
Write-Host "  Файл: $CatalogPath"
Write-Host "  Ключей: $($stringProperties.Count)"
Write-Host "  Языков: $($requiredLanguages.Count)"
