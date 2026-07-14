<#
.SYNOPSIS
Проверяет, что portable/staging-пакет содержит runtime JSON-локализацию.

.DESCRIPTION
После package-portable.ps1 каталог out\package\FictionBookEditor должен содержать
Lang/<locale>/<module>.json для FBE, FBV и всех текущих плагинов. Этот тест
ловит ситуацию, когда runtime JSON-локализация проходит unit-smoke, но не попала
в пакет или установщик.
#>
[CmdletBinding()]
param(
    [string]$PackageDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repoRoot "out\package\FictionBookEditor"
}

if (-not (Test-Path -LiteralPath $PackageDirectory -PathType Container)) {
    throw "Не найден portable/staging-каталог для проверки Lang: $PackageDirectory"
}

$langRoot = Join-Path $PackageDirectory "Lang"
if (-not (Test-Path -LiteralPath $langRoot -PathType Container)) {
    throw "В portable/staging-каталоге отсутствует runtime-локализация: $langRoot"
}

$expectedLanguages = @(
    "en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES",
    "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG"
)
$expectedModules = @(
    "fbe.json",
    "fbv.json",
    "export-html.json",
    "export-docx.json",
    "export-epub.json",
    "import-epub.json"
)

foreach ($language in $expectedLanguages) {
    $languageDir = Join-Path $langRoot $language
    if (-not (Test-Path -LiteralPath $languageDir -PathType Container)) {
        throw "В runtime Lang отсутствует каталог языка: $languageDir"
    }

    foreach ($moduleFile in $expectedModules) {
        $jsonPath = Join-Path $languageDir $moduleFile
        if (-not (Test-Path -LiteralPath $jsonPath -PathType Leaf)) {
            throw "В runtime Lang отсутствует файл модуля: $jsonPath"
        }

        $raw = Get-Content -Raw -LiteralPath $jsonPath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            throw "Runtime JSON-файл пуст: $jsonPath"
        }

        $json = $raw | ConvertFrom-Json -Depth 30
        if (-not $json.locale -or [string]$json.locale -ne $language) {
            throw "Runtime JSON-файл $jsonPath содержит неверную locale: $($json.locale)"
        }
        if (-not $json.module -or [string]::IsNullOrWhiteSpace([string]$json.module)) {
            throw "Runtime JSON-файл $jsonPath не содержит module."
        }
        if (-not $json.strings -or $json.strings.PSObject.Properties.Count -eq 0) {
            throw "Runtime JSON-файл $jsonPath не содержит строк."
        }
    }
}

foreach ($localizedResource in @(
    "ru-RU\\res_rus.dll",
    "uk-UA\\res_ukr.dll"
)) {
    $resourcePath = Join-Path $langRoot $localizedResource
    if (-not (Test-Path -LiteralPath $resourcePath -PathType Leaf)) {
        throw "В runtime Lang отсутствует DLL локализованных ресурсов FBE: $resourcePath"
    }
}

foreach ($legacyRootResource in @("res_rus.dll", "res_ukr.dll")) {
    $legacyRootPath = Join-Path $PackageDirectory $legacyRootResource
    if (Test-Path -LiteralPath $legacyRootPath -PathType Leaf) {
        throw "DLL локализованных ресурсов FBE не должна лежать в корне пакета: $legacyRootPath"
    }
}

Write-Host "Runtime JSON-локализация присутствует в portable/staging-пакете."
Write-Host "  Каталог: $PackageDirectory"
Write-Host "  Языков: $($expectedLanguages.Count)"
Write-Host "  Модулей: $($expectedModules.Count)"
