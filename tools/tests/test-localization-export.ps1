<#
.SYNOPSIS
Проверяет экспорт подготовительных локализационных файлов для переводчиков.

.DESCRIPTION
Скрипт запускает `tools/localization/export-weblate-seed.ps1` во временный
каталог и проверяет, что для каждого целевого языка создан JSON-файл со строками
из `app-ui`, `plugin-ui` и `installer-ui`. Это страхует будущую подготовку
Weblate от поломок.
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
    if ($manifest.formatVersion -ne 1) {
        throw "manifest.json содержит неверный formatVersion: $($manifest.formatVersion)."
    }
    if ($manifest.fallbackLanguage -ne "en-US") {
        throw "manifest.json содержит неверный fallbackLanguage: $($manifest.fallbackLanguage)."
    }
    $languages = @($manifest.languages)
    if ($languages.Count -ne 12) {
        throw "Ожидалось 12 языков экспорта, фактически: $($languages.Count)."
    }
    if ($manifest.stringCount -lt 200) {
        throw "manifest.json содержит подозрительно малый stringCount: $($manifest.stringCount)."
    }
    if (@($manifest.files).Count -ne $languages.Count) {
        throw "manifest.json содержит неверное число файлов: $(@($manifest.files).Count)."
    }

    foreach ($language in $languages) {
        $filePath = Join-Path $outputDirectory "$language.json"
        if (-not (Test-Path -LiteralPath $filePath)) {
            throw "Не создан файл экспорта для $language."
        }

        $data = Get-Content -Raw -LiteralPath $filePath | ConvertFrom-Json -Depth 30
        if ($data.formatVersion -ne 1) {
            throw "В $language.json указан неверный formatVersion: $($data.formatVersion)."
        }
        if ($data.language -ne $language) {
            throw "В $language.json указан неверный язык: $($data.language)."
        }
        if ($data.fallbackLanguage -ne "en-US") {
            throw "В $language.json указан неверный fallbackLanguage: $($data.fallbackLanguage)."
        }

        $strings = $data.strings.PSObject.Properties
        if ($data.stringCount -ne @($strings).Count) {
            throw "В $language.json stringCount не совпадает с фактическим числом строк."
        }
        if (-not ($strings.Name -contains "fbv.validation.no_errors")) {
            throw "В $language.json нет строки fbv.validation.no_errors."
        }
        if (-not ($strings.Name -contains "export_epub.dialog.options.caption")) {
            throw "В $language.json нет строки export_epub.dialog.options.caption."
        }
        if (-not ($strings.Name -contains "export_epub.summary.saved")) {
            throw "В $language.json нет строки export_epub.summary.saved."
        }
        if (-not ($strings.Name -contains "import_epub.plugin.filedlg_title")) {
            throw "В $language.json нет строки import_epub.plugin.filedlg_title."
        }
        if (-not ($strings.Name -contains "nsis.FinishPageRunText")) {
            throw "В $language.json нет строки nsis.FinishPageRunText."
        }
        $installerFinishString = $strings["nsis.FinishPageRunText"].Value
        if ($language -in @("en-US", "ru-RU", "uk-UA")) {
            if ($installerFinishString.needsTranslation) {
                throw "Для $language.json ошибочно помечен английский fallback установщика."
            }
        } elseif (-not $installerFinishString.needsTranslation) {
            throw "Для $language.json не помечена необходимость вычитки строки установщика."
        }
    }

    Write-Host "Экспорт локализационных seed-файлов прошёл проверку."
    Write-Host "  Каталог: $outputDirectory"
}
finally {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
