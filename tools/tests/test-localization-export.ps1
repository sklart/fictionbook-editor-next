<#
.SYNOPSIS
Проверяет экспорт подготовительных локализационных файлов для переводчиков.

.DESCRIPTION
Скрипт запускает `tools/localization/export-weblate-seed.ps1` во временный
каталог и проверяет, что для каждого целевого языка создан JSON-файл со строками
из `app-ui` и `plugin-ui`. Это страхует будущую подготовку Weblate от поломок.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-localization-export-$PID"

try {
    & (Join-Path $repoRoot "tools\localization\export-weblate-seed.ps1") -OutputDirectory $outputDirectory | Out-Host

    $manifestPath = Join-Path $outputDirectory "manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath)) {
        throw "Экспорт не создал manifest.json."
    }

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json -Depth 20
    $languages = @($manifest.languages)
    if ($languages.Count -ne 12) {
        throw "Ожидалось 12 языков экспорта, фактически: $($languages.Count)."
    }

    foreach ($language in $languages) {
        $filePath = Join-Path $outputDirectory "$language.json"
        if (-not (Test-Path -LiteralPath $filePath)) {
            throw "Не создан файл экспорта для $language."
        }

        $data = Get-Content -Raw -LiteralPath $filePath | ConvertFrom-Json -Depth 30
        if ($data.language -ne $language) {
            throw "В $language.json указан неверный язык: $($data.language)."
        }

        $strings = $data.strings.PSObject.Properties
        if (-not ($strings.Name -contains "fbv.validation.no_errors")) {
            throw "В $language.json нет строки fbv.validation.no_errors."
        }
        if (-not ($strings.Name -contains "export_epub.dialog.options.caption")) {
            throw "В $language.json нет строки export_epub.dialog.options.caption."
        }
    }

    Write-Host "Экспорт локализационных seed-файлов прошёл проверку."
    Write-Host "  Каталог: $outputDirectory"
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
