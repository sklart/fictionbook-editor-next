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
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
Set-StrictMode -Off
$scratch = New-IsolatedFbeScratchDirectory -Name 'localization-rc2'
$outputDirectory = $scratch.Root
$passed = $false

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

    $manifest = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
    $languages = @($manifest.languages)
    if ($languages.Count -ne 12) {
        throw "Ожидалось 12 языков, фактически: $($languages.Count)."
    }

    $header = Get-Content -Raw -LiteralPath $headerPath
    # IDs follow the ordered catalog and can move when catalog entries are
    # added; verify the generated identifier and its range, not an old index.
    if ($header -notmatch 'IDS_L10N_APP_FBE_UPDATE_CHECKING\s+(\d+)') {
        throw "Header не содержит ожидаемый app-ui идентификатор."
    }
    $updateId = [int]$Matches[1]
    $appScope = @($manifest.catalogs | Where-Object { $_.scope -eq 'app-ui' })
    if ($appScope.Count -ne 1 -or $updateId -lt [int]$appScope[0].firstId -or $updateId -gt [int]$appScope[0].lastId) { throw 'Идентификатор app-ui находится вне диапазона manifest.' }
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
    if ($ukApp -notmatch 'IDS_L10N_APP_FBE_UPDATE_CHECKING\s+L"') { throw 'Идентификатор обновления отсутствует в украинском app-ui.rc2.' }
    if ($ukApp -notmatch 'Файл доступний лише для читання') {
        throw "Украинская строка read-only warning не попала в app-ui.uk-UA.rc2."
    }

    $allRcText = Get-ChildItem -LiteralPath $outputDirectory -Filter *.rc2 |
        ForEach-Object { [IO.File]::ReadAllText($_.FullName, [Text.Encoding]::Unicode) } |
        Out-String

     # Portuguese words such as “Âmbito” are valid text, not UTF-8 mojibake.
     $mojibake = ([char]0xFFFD) + '|[ÐÑÃÂ][\u0080-\u00BF]'
     if ('Âmbito' -cmatch $mojibake -or ("Ð" + [char]0x009F) -cnotmatch $mojibake) { throw 'Проверка mojibake не различает допустимый текст и повреждённый UTF-8.' }
     if ($allRcText -cmatch $mojibake) {
        throw "В сгенерированных .rc2 обнаружены признаки mojibake."
    }

    Write-Host "Генерация Win32 resource-фрагментов локализации прошла проверку."
    $passed = $true
}
finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
