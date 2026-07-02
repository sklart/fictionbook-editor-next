<#
.SYNOPSIS
Проверяет подготовительный каталог локализации основного интерфейса FBE/FBV.

.DESCRIPTION
Скрипт валидирует `localization/app-ui/catalog.json`: наличие целевых языков,
существование исходных ресурсных файлов, стабильность ключей и заполненность
стартовых переводов. Каталог пока не участвует в runtime-сборке, но должен
оставаться машинно-проверяемым, чтобы его можно было позже отдать в Weblate или
генерировать из него Win32 resource-фрагменты.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"

if (-not (Test-Path -LiteralPath $catalogPath)) {
    throw "Не найден каталог локализации FBE/FBV: $catalogPath"
}

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 20

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

$actualLanguages = @($catalog.targetLanguages)
foreach ($language in $requiredLanguages) {
    if ($language -notin $actualLanguages) {
        throw "В каталоге FBE/FBV отсутствует целевой язык: $language"
    }
}

if (-not $catalog.components) {
    throw "В каталоге FBE/FBV отсутствует раздел components."
}

$components = $catalog.components.PSObject.Properties
foreach ($component in $components) {
    if ([string]::IsNullOrWhiteSpace($component.Name)) {
        throw "В каталоге FBE/FBV найден компонент с пустым ключом."
    }

    $sources = $component.Value.sources
    if (-not $sources) {
        throw "У компонента $($component.Name) отсутствует sources."
    }

    foreach ($source in $sources.PSObject.Properties) {
        $relativePath = [string]$source.Value
        if ([string]::IsNullOrWhiteSpace($relativePath)) {
            throw "У компонента $($component.Name) указан пустой путь source."
        }

        $sourcePath = Join-Path $repoRoot $relativePath
        if (-not (Test-Path -LiteralPath $sourcePath)) {
            throw "Источник компонента $($component.Name) не найден: $relativePath"
        }
    }
}

if (-not $catalog.seedStrings) {
    throw "В каталоге FBE/FBV отсутствует раздел seedStrings."
}

$keyPattern = '^(fbe|fbv|installer)\.[a-z0-9_]+(\.[a-z0-9_]+)*$'
$seedStrings = @($catalog.seedStrings.PSObject.Properties)
foreach ($entry in $seedStrings) {
    if ($entry.Name -notmatch $keyPattern) {
        throw "Некорректный ключ строки FBE/FBV: $($entry.Name)"
    }

    if ([string]::IsNullOrWhiteSpace([string]$entry.Value.resourceId)) {
        throw "У строки $($entry.Name) отсутствует resourceId."
    }

    if ([string]::IsNullOrWhiteSpace([string]$entry.Value.component)) {
        throw "У строки $($entry.Name) отсутствует component."
    }

    if (-not ($components.Name -contains [string]$entry.Value.component)) {
        throw "Строка $($entry.Name) ссылается на неизвестный компонент: $($entry.Value.component)"
    }

    $translations = $entry.Value.translations
    if (-not $translations) {
        throw "У строки $($entry.Name) отсутствуют translations."
    }

    foreach ($language in $requiredLanguages) {
        $property = $translations.PSObject.Properties[$language]
        if (-not $property -or [string]::IsNullOrWhiteSpace([string]$property.Value)) {
            throw "У строки $($entry.Name) отсутствует перевод для $language."
        }
    }
}

$requiredFbeResources = @(
    "IDS_UPDATE_CHECK",
    "IDS_UPDATE_DOWNLOADERROR",
    "IDS_UPDATE_HAVELATESTVERSION",
    "IDS_READONLY_SAVE_MSG",
    "IDD_ABOUTBOX"
)
foreach ($resourceFile in @("src\locales\res_rus\FBE.rc", "src\locales\res_ukr\FBE.rc")) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $resourceFile)
    foreach ($resourceName in $requiredFbeResources) {
        if ($text -notmatch [regex]::Escape($resourceName)) {
            throw "В $resourceFile не найден обязательный ресурс $resourceName."
        }
    }
}

$fbvRc = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbv\FBV.rc")
if ($fbvRc -notmatch "IDS_SHELL_VALIDATE_VERB") {
    throw "В src\fbv\FBV.rc не найден IDS_SHELL_VALIDATE_VERB."
}

Write-Host "Каталог локализации FBE/FBV прошёл проверку."
Write-Host "  Файл: $catalogPath"
Write-Host "  Строк: $($seedStrings.Count)"
Write-Host "  Языков: $($requiredLanguages.Count)"
