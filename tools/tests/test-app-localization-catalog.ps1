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

    if ([string]::IsNullOrWhiteSpace([string]$entry.Value.resourceId) -and -not [bool]$entry.Value.runtimeOnly) {
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

    $sourceText = [string]$entry.Value.source
    $brandFragments = @(
        "FictionBook Editor Next",
        "FB Editor Next",
        "FictionBook Validator Next",
        "FictionBook Editor",
        "FictionBook Validator"
    )
    foreach ($brand in $brandFragments) {
        if ($sourceText -notlike "*$brand*") {
            continue
        }

        foreach ($language in $requiredLanguages) {
            $translatedText = [string]$translations.PSObject.Properties[$language].Value
            if ($translatedText -notlike "*$brand*") {
                throw "Брендовое имя '$brand' нельзя переводить или менять. Строка: $($entry.Name), язык: $language"
            }
        }
    }
}

$fbeResourceHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\resource.h")
$requiredFbeResources = @(
    "IDS_UPDATE_CHECK",
    "IDS_UPDATE_CONNECTING",
    "IDS_UPDATE_CANTCONNECT",
    "IDS_UPDATE_DOWNLOADCOMPLETE",
    "IDS_UPDATE_DOWNLOADERROR",
    "IDS_UPDATE_404ERROR",
    "IDS_UPDATE_403ERROR",
    "IDS_UPDATE_407ERROR",
    "IDS_UPDATE_NOTSUPPORTEDRANGE",
    "IDS_UPDATE_DOWNLOADERRORSTATUS",
    "IDS_UPDATE_INCORRECTMD5",
    "IDS_UPDATE_NEWVERSIONAVAILABLE",
    "IDS_UPDATE_HAVELATESTVERSION",
    "IDS_UPDATE_DOWNLOADEDFROM",
    "IDS_UPDATE_DOWNLOADED",
    "IDS_UPDATE_DOWNLOADREADY",
    "IDS_UPDATE_CLOSE",
    "IDS_SEARCH_END_MSG",
    "IDS_READONLY_SAVE_MSG",
    "IDD_ABOUTBOX"
)
$requiredGeneratedFbeStringResources = @(
    $requiredFbeResources | Where-Object { $_ -match '^IDS_' }
)

$fbeCatalogResources = @(
    $seedStrings |
        Where-Object { [string]$_.Value.component -eq "fbe.core" } |
        ForEach-Object { [string]$_.Value.resourceId }
)

foreach ($resourceName in $fbeCatalogResources) {
    if ($resourceName -match '^IDS_' -and $fbeResourceHeader -notmatch [regex]::Escape($resourceName)) {
        throw "Каталог FBE ссылается на отсутствующий ресурс src\fbe\resource.h: $resourceName."
    }
}

$fbeMainResourceFile = "src\fbe\FBE.rc"
$fbeMainResourceText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $fbeMainResourceFile)
foreach ($resourceName in $requiredFbeResources) {
    if ($fbeMainResourceText -notmatch [regex]::Escape($resourceName)) {
        throw "В $fbeMainResourceFile не найден обязательный ресурс $resourceName."
    }
}

$localizedFbeResourceFiles = @(
    @{
        ResourceFile = "src\locales\res_rus\FBE.rc"
        GeneratedFile = "src\locales\res_rus\FBEStrings.generated.rc2"
    },
    @{
        ResourceFile = "src\locales\res_ukr\FBE.rc"
        GeneratedFile = "src\locales\res_ukr\FBEStrings.generated.rc2"
    }
)

foreach ($resourcePair in $localizedFbeResourceFiles) {
    $resourceText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $resourcePair.ResourceFile)
    $generatedText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $resourcePair.GeneratedFile)
    if ($resourceText -notmatch '#include\s+"FBEStrings\.generated\.rc2"') {
        throw "В $($resourcePair.ResourceFile) не подключён FBEStrings.generated.rc2."
    }

    foreach ($resourceName in $requiredGeneratedFbeStringResources) {
        if ($generatedText -notmatch [regex]::Escape($resourceName)) {
            throw "В $($resourcePair.GeneratedFile) не найден обязательный ресурс $resourceName."
        }
    }
}

foreach ($resourceName in $requiredFbeResources) {
    if ($resourceName -notin $fbeCatalogResources) {
        Write-Warning "FBE-ресурс $resourceName ещё не заведён в localization/app-ui/catalog.json."
    }
}

$fbvResourceHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbv\resource.h")
$fbvGeneratedStrings = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbv\FBVStrings.generated.rc2")
if ($fbvResourceHeader -notmatch "IDS_SHELL_VALIDATE_VERB") {
    throw "В src\fbv\resource.h не найден IDS_SHELL_VALIDATE_VERB."
}
if ($fbvGeneratedStrings -notmatch "IDS_SHELL_VALIDATE_VERB") {
    throw "В src\fbv\FBVStrings.generated.rc2 не найден IDS_SHELL_VALIDATE_VERB."
}

Write-Host "Каталог локализации FBE/FBV прошёл проверку."
Write-Host "  Файл: $catalogPath"
Write-Host "  Строк: $($seedStrings.Count)"
Write-Host "  Языков: $($requiredLanguages.Count)"
