<#
.SYNOPSIS
Проверяет генерацию NSIS-секций языковых пакетов.

.DESCRIPTION
Скрипт запускает `tools/localization/export-nsis-language-pack-plan.ps1` во
временный файл и проверяет, что generated `.nsh` содержит все целевые языки,
правильные флаги выбора и ключевые языковые ресурсы.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-nsis-language-plan-$PID"
$outputPath = Join-Path $outputDirectory "LanguagePacks.generated.nsh"

try {
    & (Join-Path $repoRoot "tools\localization\export-nsis-language-pack-plan.ps1") -OutputPath $outputPath | Out-Host

    if (-not (Test-Path -LiteralPath $outputPath)) {
        throw "Генератор не создал generated .nsh: $outputPath"
    }

    $text = Get-Content -Raw -LiteralPath $outputPath
    foreach ($language in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG")) {
        if ($text -notmatch [regex]::Escape($language)) {
            throw "В draft .nsh отсутствует язык: $language"
        }
    }

    foreach ($asset in @("res_rus.dll", "res_ukr.dll", "FBVVerbResources.dll.mui", "Lang\en-US", "Lang\ru-RU", "Lang\uk-UA")) {
        if ($text -notmatch [regex]::Escape($asset)) {
            throw "В generated .nsh отсутствует ожидаемый ресурс: $asset"
        }
    }

    foreach ($sectionName in @("LanguagePack_en_US", "LanguagePack_ru_RU", "LanguagePack_uk_UA")) {
        if ($text -notmatch ('Section (?!/o ).*' + [regex]::Escape($sectionName))) {
            throw "Язык по умолчанию ошибочно сделан необязательным: $sectionName"
        }
    }

    if ($text -notmatch 'Section "English \(en-US\)" LanguagePack_en_US\s+;[^\r\n]*\s+SectionIn RO') {
        throw "Английский fallback должен быть обязательным языковым компонентом."
    }

    Write-Host "Генерация NSIS-секций языковых пакетов прошла проверку."
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
