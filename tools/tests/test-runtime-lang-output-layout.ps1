<#
.SYNOPSIS
    Проверяет размещение runtime-локализации рядом с бинарниками FBE.
.DESCRIPTION
    Контролирует, что все внешние JSON-пакеты и локализованные resource DLL
    лежат только в Lang/<locale>. Старые пустые каталоги <locale> в корне
    out/<Configuration> не должны возвращаться после сборки.
#>

[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "out\\$Configuration"
}

if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    throw "Не найден каталог результатов сборки: $OutputDirectory"
}

$locales = @(
    "en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES",
    "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG"
)
$expectedModules = @(
    "fbe.json", "fbv.json", "export-html.json", "export-docx.json",
    "export-epub.json", "import-epub.json"
)

$langRoot = Join-Path $OutputDirectory "Lang"
if (-not (Test-Path -LiteralPath $langRoot -PathType Container)) {
    throw "Не найден runtime-каталог локализации: $langRoot"
}

foreach ($locale in $locales) {
    $legacyDirectory = Join-Path $OutputDirectory $locale
    if (Test-Path -LiteralPath $legacyDirectory -PathType Container) {
        throw "Устаревший корневой каталог локали не должен присутствовать: $legacyDirectory. Используйте Lang\\$locale."
    }

    $languageDirectory = Join-Path $langRoot $locale
    if (-not (Test-Path -LiteralPath $languageDirectory -PathType Container)) {
        throw "В Lang отсутствует каталог языка: $languageDirectory"
    }

    foreach ($module in $expectedModules) {
        $modulePath = Join-Path $languageDirectory $module
        if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) {
            throw "В Lang отсутствует JSON-модуль локализации: $modulePath"
        }
    }
}

foreach ($obsolete in @("ru-RU\\res_rus.dll", "uk-UA\\res_ukr.dll", "ru-RU\\res_rus.pdb", "uk-UA\\res_ukr.pdb")) {
    $path = Join-Path $langRoot $obsolete
    if (Test-Path -LiteralPath $path -PathType Leaf) { throw "Устаревший FBE locale artifact не должен присутствовать: $path" }
}

Write-Host "Runtime-локализация в каталоге результатов размещена корректно."
Write-Host "  Каталог: $OutputDirectory"
Write-Host "  Языков: $($locales.Count)"
