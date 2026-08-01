<#
.SYNOPSIS
Проверяет JSON-каталог главного меню FBE IDR_MAINFRAME.

.DESCRIPTION
Тест страхует подготовительный Weblate-friendly каталог меню FBE: проверяет набор
языков, наличие переводов для каждого пункта, непустые строки и сохранение
клавиатурного акселератора `&` хотя бы в базовых языках ru-RU/uk-UA/en-US.
Каталог пока не подключён к runtime, но нужен как следующий слой миграции меню
из Win32 .rc в JSON/generation pipeline.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-idr-mainframe-menu.json"
if (-not (Test-Path -LiteralPath $catalogPath)) {
    throw "Каталог главного меню FBE не найден: $catalogPath"
}

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 40
$expectedLanguages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
$languages = @($catalog.targetLanguages)
if ((Compare-Object -ReferenceObject $expectedLanguages -DifferenceObject $languages).Count -ne 0) {
    throw "Набор языков каталога главного меню FBE не совпадает с ожидаемым."
}

if ($catalog.resource -ne 'IDR_MAINFRAME' -or $catalog.resourceType -ne 'MENU') {
    throw "Каталог описывает не IDR_MAINFRAME MENU."
}

$entries = @($catalog.strings.PSObject.Properties)
if ($entries.Count -lt 1) {
    throw "Каталог главного меню FBE не содержит строк."
}

foreach ($entry in $entries) {
    foreach ($language in $expectedLanguages) {
        $translation = $entry.Value.translations.PSObject.Properties[$language]
        if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) {
            throw "У пункта $($entry.Name) нет перевода для $language."
        }
    }

    if ($entry.Value.kind -eq 'POPUP') {
        foreach ($language in @('en-US','ru-RU','uk-UA')) {
            $value = [string]$entry.Value.translations.PSObject.Properties[$language].Value
            if ($value -notmatch '&') {
                throw "У POPUP-пункта $($entry.Name) в $language потерян menu accelerator &: $value"
            }
        }
    }
}

foreach ($requiredKey in @(
    'fbe.menu.idr_mainframe.popup.file',
    'fbe.menu.idr_mainframe.file.open',
    'fbe.menu.idr_mainframe.popup.edit',
    'fbe.menu.idr_mainframe.popup.view',
    'fbe.menu.idr_mainframe.popup.insert',
    'fbe.menu.idr_mainframe.popup.style',
    'fbe.menu.idr_mainframe.popup.tools',
    'fbe.menu.idr_mainframe.tools.diagnostic_trace',
    'fbe.menu.idr_mainframe.popup.help'
)) {
    if (-not $catalog.strings.PSObject.Properties[$requiredKey]) {
        throw "В каталоге нет обязательного пункта: $requiredKey"
    }
}

Write-Host "Каталог главного меню FBE прошёл проверку."
Write-Host "  Файл: $catalogPath"
Write-Host "  Пунктов: $($entries.Count)"
Write-Host "  Языков: $($languages.Count)"
