<#
.SYNOPSIS
Проверяет генерацию Win32 resource-фрагментов из JSON-каталогов локализации.

.DESCRIPTION
Скрипт запускает `tools/localization/export-win32-resource-fragments.ps1` во
временный каталог и проверяет, что создаются общий header, `.rc2` для всех
целевых языков, корректные Unicode-строки и стабильные `IDS_L10N_*`
идентификаторы. Это промежуточная страховка перед подключением JSON-каталогов к
реальным Win32 resource-файлам.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-localization-rc2-$PID"

try {
    & (Join-Path $repoRoot "tools\localization\export-win32-resource-fragments.ps1") -OutputDirectory $outputDirectory | Out-Host

    $manifestPath = Join-Path $outputDirectory "manifest.json"
    $headerPath = Join-Path $outputDirectory "l10n_resource_ids.h"
    if (-not (Test-Path -LiteralPath $manifestPath)) {
        throw "Генератор не создал manifest.json."
    }
    if (-not (Test-Path -LiteralPath $headerPath)) {
        throw "Генератор не создал l10n_resource_ids.h."
    }

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json -Depth 20
    $languages = @($manifest.languages)
    if ($languages.Count -ne 12) {
        throw "Ожидалось 12 языков, фактически: $($languages.Count)."
    }

    $header = Get-Content -Raw -LiteralPath $headerPath
    if ($header -notmatch 'IDS_L10N_APP_FBE_UPDATE_CHECKING\s+70001') {
        throw "Header не содержит ожидаемый app-ui идентификатор."
    }
    if ($header -notmatch 'IDS_L10N_PLUGIN_EXPORT_EPUB_DIALOG_OPTIONS_CAPTION\s+\d+') {
        throw "Header не содержит ожидаемый plugin-ui идентификатор."
    }

    foreach ($language in $languages) {
        foreach ($scope in @("app-ui", "plugin-ui")) {
            $rcPath = Join-Path $outputDirectory "$scope.$language.rc2"
            if (-not (Test-Path -LiteralPath $rcPath)) {
                throw "Не создан файл $scope.$language.rc2."
            }

            $bytes = [IO.File]::ReadAllBytes($rcPath)
            if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE) {
                throw "$scope.$language.rc2 должен быть UTF-16 LE с BOM для rc.exe."
            }
        }
    }

    $ruPlugin = [IO.File]::ReadAllText((Join-Path $outputDirectory "plugin-ui.ru-RU.rc2"), [Text.Encoding]::Unicode)
    if ($ruPlugin -notmatch 'IDS_L10N_PLUGIN_EXPORT_EPUB_DIALOG_OPTIONS_CAPTION\s+L"Параметры экспорта EPUB"') {
        throw "Русская строка ExportEPUB не попала в plugin-ui.ru-RU.rc2."
    }

    $ukApp = [IO.File]::ReadAllText((Join-Path $outputDirectory "app-ui.uk-UA.rc2"), [Text.Encoding]::Unicode)
    if ($ukApp -notmatch 'Файл доступний лише для читання') {
        throw "Украинская строка read-only warning не попала в app-ui.uk-UA.rc2."
    }

    $allRcText = Get-ChildItem -LiteralPath $outputDirectory -Filter *.rc2 |
        ForEach-Object { [IO.File]::ReadAllText($_.FullName, [Text.Encoding]::Unicode) } |
        Out-String

    if ($allRcText -cmatch '�|Ð.|Ñ.|Ã.|Â.') {
        throw "В сгенерированных .rc2 обнаружены признаки mojibake."
    }

    Write-Host "Генерация Win32 resource-фрагментов локализации прошла проверку."
    Write-Host "  Каталог: $outputDirectory"
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
