<#
.SYNOPSIS
Проверяет генерацию чернового NSIS-плана языковых пакетов.

.DESCRIPTION
Скрипт запускает `tools/localization/export-nsis-language-pack-plan.ps1` во
временный каталог и проверяет, что draft `.nsh` содержит все целевые языки и
ключевые языковые ресурсы. Сам draft-файл не подключается к установщику.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-nsis-language-plan-$PID"

try {
    & (Join-Path $repoRoot "tools\localization\export-nsis-language-pack-plan.ps1") -OutputDirectory $outputDirectory | Out-Host

    $draftPath = Join-Path $outputDirectory "FictionBookEditorNext.LanguagePacks.draft.nsh"
    if (-not (Test-Path -LiteralPath $draftPath)) {
        throw "Генератор не создал draft .nsh: $draftPath"
    }

    $text = Get-Content -Raw -LiteralPath $draftPath
    foreach ($language in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG")) {
        if ($text -notmatch [regex]::Escape($language)) {
            throw "В draft .nsh отсутствует язык: $language"
        }
    }

    foreach ($asset in @("res_rus.dll", "res_ukr.dll", "FBVVerbResources.dll.mui", "Lang\Shell\FBVVerbResources.dll", "Lang\Shell\ru-RU\FBVVerbResources.dll.mui", "gpl-3.0.ru.txt", "rus.xsl", "ukr.xsl")) {
        if ($text -notmatch [regex]::Escape($asset)) {
            throw "В draft .nsh отсутствует ожидаемый ресурс: $asset"
        }
    }

    Write-Host "Генерация чернового NSIS-плана языковых пакетов прошла проверку."
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
