<#
.SYNOPSIS
Проверяет generated-ресурсы локализации FBE.

.DESCRIPTION
Скрипт запускает генератор `update-fbe-resource-strings.ps1`, затем сверяет
`src/locales/res_rus/FBEStrings.generated.rc2` и
`src/locales/res_ukr/FBEStrings.generated.rc2` с `localization/app-ui/catalog.json`.
Дополнительно проверяется, что generated `.rc2` подключены в локализованные
`FBE.rc`, а перенесённые строки не остались там ручными дублями.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"
$generatorPath = Join-Path $repoRoot "tools\localization\update-fbe-resource-strings.ps1"

& $generatorPath | Out-Host

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 30
$entries = @(
    $catalog.seedStrings.PSObject.Properties |
        Where-Object {
            [string]$_.Value.component -eq "fbe.core" -and
                [string]$_.Value.resourceId -match '^IDS_[A-Z0-9_]+$'
        }
)

if ($entries.Count -eq 0) {
    throw "В app-ui catalog не найдены FBE-строки для generated resource-файлов."
}

$files = @(
    @{
        Language = "ru-RU"
        Path = Join-Path $repoRoot "src\locales\res_rus\FBEStrings.generated.rc2"
        RcPath = Join-Path $repoRoot "src\locales\res_rus\FBE.rc"
        ExpectedLanguage = "LANGUAGE LANG_RUSSIAN, SUBLANG_DEFAULT"
    },
    @{
        Language = "uk-UA"
        Path = Join-Path $repoRoot "src\locales\res_ukr\FBEStrings.generated.rc2"
        RcPath = Join-Path $repoRoot "src\locales\res_ukr\FBE.rc"
        ExpectedLanguage = "LANGUAGE LANG_UKRAINIAN, SUBLANG_DEFAULT"
    }
)

foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file.Path)) {
        throw "Generated FBE resource-файл не найден: $($file.Path)"
    }
    if (-not (Test-Path -LiteralPath $file.RcPath)) {
        throw "Локализованный FBE.rc не найден: $($file.RcPath)"
    }

    $text = Get-Content -Raw -LiteralPath $file.Path
    $rcText = Get-Content -Raw -LiteralPath $file.RcPath

    if ($rcText -notmatch '#include\s+"FBEStrings\.generated\.rc2"') {
        throw "В $($file.RcPath) не подключён FBEStrings.generated.rc2."
    }

    if ($text -notmatch [regex]::Escape($file.ExpectedLanguage)) {
        throw "В $($file.Path) отсутствует ожидаемая директива языка: $($file.ExpectedLanguage)"
    }

    if ($text -match '�|Ð|Ñ|Рџ|Рђ|╨|╤') {
        throw "В $($file.Path) обнаружены признаки mojibake."
    }

    foreach ($entry in $entries) {
        $resourceId = [string]$entry.Value.resourceId
        $translation = $entry.Value.translations.PSObject.Properties[$file.Language]
        if (-not $translation) {
            throw "У FBE-строки $($entry.Name) отсутствует перевод $($file.Language)."
        }

        if ($text -notmatch [regex]::Escape($resourceId)) {
            throw "В $($file.Path) отсутствует ресурс $resourceId."
        }

        if ($rcText -match "(?m)^\s*$([regex]::Escape($resourceId))\b") {
            throw "В $($file.RcPath) остался ручной дубль ресурса $resourceId."
        }
    }
}

Write-Host "Generated-ресурсы локализации FBE подключены и прошли проверку."
Write-Host "  Строк на язык: $($entries.Count)"
